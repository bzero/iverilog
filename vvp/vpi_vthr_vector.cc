/*
 * Copyright (c) 2001-2014 Stephen Williams (steve@icarus.com)
 * Copyright (c) 2001 Stephan Boettcher <stephan@nevis.columbia.edu>
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * vpiReg handles are handled here. These objects represent vectors of
 * .var objects that can be manipulated by the VPI module.
 */

# include  "vpi_priv.h"
# include  "vthread.h"
# include  "config.h"
#ifdef CHECK_WITH_VALGRIND
# include  "vvp_cleanup.h"
# include <map>
#endif
# include  <cstdio>
# include  <cstdlib>
# include  <cassert>
# include  "ivl_alloc.h"

/*
 * Hex digits that represent 4-value bits of Verilog are not as
 * trivially obvious to display as if the bits were the usual 2-value
 * bits. So, although it is possible to write a function that
 * generates a correct character for 4*4-value bits, it is easier to
 * just perform the lookup in a table. This only takes 256 bytes,
 * which is not many executable instructions:-)
 *
 * The table is calculated as compile time, therefore, by the
 * draw_tt.c program.
 */
extern const char hex_digits[256];
extern const char oct_digits[64];



#ifdef CHECK_WITH_VALGRIND
static map<vpiHandle, bool> handle_map;

void thread_vthr_delete(vpiHandle item)
{
      handle_map[item] = true;
}

static void thread_vthr_delete_real(vpiHandle item)
{
      struct __vpiVThrVec*obj = dynamic_cast<__vpiVThrVec*>(item);
      delete obj;
}
#endif

struct __vpiVThrWord : public __vpiHandle {
      __vpiVThrWord();
      int get_type_code(void) const;
      int vpi_get(int code);
      void vpi_get_value(p_vpi_value val);

      const char* name;
      int subtype;
      unsigned index;
};

static int vthr_word_get(int code, vpiHandle ref)
{
      struct __vpiVThrWord*rfp = dynamic_cast<__vpiVThrWord*>(ref);

      switch (code) {

	  case vpiConstType:
	    return rfp->subtype;

#if defined(CHECK_WITH_VALGRIND) || defined(BR916_STOPGAP_FIX)
	  case _vpiFromThr:
	    return _vpiWord;
#endif

	  default:
	    return 0;
      }
}

static void vthr_real_get_value(vpiHandle ref, s_vpi_value*vp)
{
      struct __vpiVThrWord*obj = dynamic_cast<__vpiVThrWord*>(ref);
      char *rbuf = (char *) need_result_buf(66, RBUF_VAL);

      double val = 0.0;

	/* Get the actual value from the index. It is possible, by the
	   way, that the vpi_get_value is called from compiletf. If
	   that's the case, there will be no current thread, and this
	   will not have access to the proper value. Punt and return a
	   0.0 value instead. */
      if (vpip_current_vthread)
	    val = vthread_get_real_stack(vpip_current_vthread, obj->index);

      switch (vp->format) {

	  case vpiObjTypeVal:
	    vp->format = vpiRealVal;
	  case vpiRealVal:
	    vp->value.real = val;
	    break;

	  case vpiIntVal:
	      /* NaN or +/- infinity are translated as 0. */
	    if (val != val || (val && (val == 0.5*val))) {
		  val = 0.0;
	    } else {
		  if (val >= 0.0) val = floor(val + 0.5);
		  else val = ceil(val - 0.5);
	    }
	    vp->value.integer = (PLI_INT32)val;
	    break;

	  case vpiDecStrVal:
	    sprintf(rbuf, "%0.0f", val);
	    vp->value.str = rbuf;
	    break;

	  case vpiHexStrVal:
	    sprintf(rbuf, "%lx", (long)val);
	    vp->value.str = rbuf;
	    break;

	  case vpiBinStrVal: {
		unsigned long vali = (unsigned long)val;
		unsigned len = 0;

		while (vali > 0) {
		      len += 1;
		      vali /= 2;
		}

		vali = (unsigned long)val;
		for (unsigned idx = 0 ;  idx < len ;  idx += 1) {
		      rbuf[len-idx-1] = (vali & 1)? '1' : '0';
		      vali /= 2;
		}

		rbuf[len] = 0;
		if (len == 0) {
		      rbuf[0] = '0';
		      rbuf[1] = 0;
		}
		vp->value.str = rbuf;
		break;
	  }

	  default:
	    fprintf(stderr, "vvp error: get %d not supported "
		      "by vpiConstant (Real)\n", (int)vp->format);

	    vp->format = vpiSuppressVal;
	    break;
      }
}

inline __vpiVThrWord::__vpiVThrWord()
{ }

int __vpiVThrWord::get_type_code(void) const
{ return vpiConstant; }

int __vpiVThrWord::vpi_get(int code)
{ return vthr_word_get(code, this); }

void __vpiVThrWord::vpi_get_value(p_vpi_value val)
{ vthr_real_get_value(this, val); }

vpiHandle vpip_make_vthr_word(unsigned base, const char*type)
{
      struct __vpiVThrWord*obj = new __vpiVThrWord;

      assert(type[0] == 'r');

      obj->name = vpip_name_string("W<>");
      obj->subtype = vpiRealConst;
      assert(base < 65536);
      obj->index = base;

      return obj;
}

#ifdef CHECK_WITH_VALGRIND
void thread_word_delete(vpiHandle item)
{
      handle_map[item] = false;
}

static void thread_word_delete_real(vpiHandle item)
{
      struct __vpiVThrWord*obj = dynamic_cast<__vpiVThrWord*>(item);
      delete obj;
}

void vpi_handle_delete()
{
      map<vpiHandle, bool>::iterator iter;
      for (iter = handle_map.begin(); iter != handle_map.end(); ++ iter ) {
	    if (iter->second) thread_vthr_delete_real(iter->first);
	    else thread_word_delete_real(iter->first);
      }
}
#endif

class __vpiVThrStrStack : public __vpiHandle {
    public:
      __vpiVThrStrStack(unsigned depth);
      int get_type_code(void) const;
      int vpi_get(int code);
      void vpi_get_value(p_vpi_value val);
    private:
      unsigned depth_;
};

__vpiVThrStrStack::__vpiVThrStrStack(unsigned d)
: depth_(d)
{
}

int __vpiVThrStrStack::get_type_code(void) const
{ return vpiConstant; }

int __vpiVThrStrStack::vpi_get(int code)
{
      switch (code) {
	  case vpiConstType:
	    return vpiStringConst;
#if defined(CHECK_WITH_VALGRIND) || defined(BR916_STOPGAP_FIX)
	  case _vpiFromThr:
	    return _vpiString;
#endif
	  default:
	    return 0;
      }
}

void __vpiVThrStrStack::vpi_get_value(p_vpi_value vp)
{
      string val;
      char*rbuf = 0;

      if (vpip_current_vthread)
	    val = vthread_get_str_stack(vpip_current_vthread, depth_);

      switch (vp->format) {

	  case vpiObjTypeVal:
	    vp->format = vpiStringVal;
	  case vpiStringVal:
	    rbuf = (char *) need_result_buf(val.size()+1, RBUF_VAL);
	    strcpy(rbuf, val.c_str());
	    vp->value.str = rbuf;
	    break;

	  default:
	    fprintf(stderr, "vvp error: get %d not supported "
		      "by vpiConstant (String)\n", (int)vp->format);

	    vp->format = vpiSuppressVal;
	    break;
      }
}

class __vpiVThrVec4Stack : public __vpiHandle {
    public:
      __vpiVThrVec4Stack(unsigned depth, bool signed_flag, unsigned wid);
      int get_type_code(void) const;
      int vpi_get(int code);
      char*vpi_get_str(int code);
      void vpi_get_value(p_vpi_value val);
      vpiHandle vpi_put_value(p_vpi_value val, int flags);
    private:
      void vpi_get_value_string_(p_vpi_value vp, const vvp_vector4_t&val);
      void vpi_get_value_binstr_(p_vpi_value vp, const vvp_vector4_t&val);
      void vpi_get_value_decstr_(p_vpi_value vp, const vvp_vector4_t&val);
      void vpi_get_value_int_   (p_vpi_value vp, const vvp_vector4_t&val);
      void vpi_get_value_real_  (p_vpi_value vp, const vvp_vector4_t&val);
      void vpi_get_value_hexstr_(p_vpi_value vp, const vvp_vector4_t&val);
      void vpi_get_value_vector_(p_vpi_value vp, const vvp_vector4_t&val);
    private:
      unsigned depth_;
      bool signed_flag_;
      unsigned expect_width_;
      const char*name;
};

__vpiVThrVec4Stack::__vpiVThrVec4Stack(unsigned d, bool sf, unsigned wid)
: depth_(d), signed_flag_(sf), expect_width_(wid)
{
      name = vpip_name_string("S<,vec4,>");
}

int __vpiVThrVec4Stack::get_type_code(void) const
{ return vpiConstant; }


int __vpiVThrVec4Stack::vpi_get(int code)
{
      switch (code) {
	  case vpiSize:
	    return expect_width_;

	  case vpiSigned:
	    return signed_flag_? 1 : 0;

	  case vpiConstType:
	    return vpiBinaryConst;

#if defined(CHECK_WITH_VALGRIND) || defined(BR916_STOPGAP_FIX)
	  case _vpiFromThr:
	    return _vpiVThr;
#endif

	  default:
	    return 0;
      }
}

char*__vpiVThrVec4Stack::vpi_get_str(int code)
{
      switch (code) {
	  case vpiFullName:
	    return simple_set_rbuf_str(name);

	  default:
	    return 0;
      }
}

void __vpiVThrVec4Stack::vpi_get_value(p_vpi_value vp)
{
      vvp_vector4_t val;

      if (vpip_current_vthread)
	    val = vthread_get_vec4_stack(vpip_current_vthread, depth_);

      switch (vp->format) {

	  case vpiBinStrVal:
	    vpi_get_value_binstr_(vp, val);
	    break;
	  case vpiDecStrVal:
	    vpi_get_value_decstr_(vp, val);
	    break;
	  case vpiHexStrVal:
	    vpi_get_value_hexstr_(vp, val);
	    break;
	  case vpiIntVal:
	    vpi_get_value_int_(vp, val);
	    break;
	  case vpiRealVal:
	    vpi_get_value_real_(vp, val);
	    break;
	  case vpiStringVal:
	    vpi_get_value_string_(vp, val);
	    break;
	  case vpiObjTypeVal:
	    vp->format = vpiVectorVal;
	  case vpiVectorVal:
	    vpi_get_value_vector_(vp, val);
	    break;

	  default:
	    fprintf(stderr, "internal error: vpi_get_value(<format=%d>)"
		    " not implemented for __vpiVThrVec4Stack.\n", vp->format);
	    assert(0);
      }

}

void __vpiVThrVec4Stack::vpi_get_value_binstr_(p_vpi_value vp, const vvp_vector4_t&val)
{
      unsigned wid = val.size();
      char*rbuf = (char*) need_result_buf(wid+1, RBUF_VAL);
      for (unsigned idx = 0 ; idx < wid ; idx += 1) {
	    rbuf[wid-idx-1] = vvp_bit4_to_ascii(val.value(idx));
      }
      rbuf[wid] = 0;
      vp->value.str = rbuf;
}

void __vpiVThrVec4Stack::vpi_get_value_decstr_(p_vpi_value vp, const vvp_vector4_t&val)
{
      unsigned wid = val.size();
      int nbuf = (wid+2)/3 + 1;
      char *rbuf = (char*) need_result_buf(nbuf, RBUF_VAL);

      vpip_vec4_to_dec_str(val, rbuf, nbuf, signed_flag_);
      vp->value.str = rbuf;
}

void __vpiVThrVec4Stack::vpi_get_value_hexstr_(p_vpi_value vp, const vvp_vector4_t&val)
{
      unsigned wid = val.size();
      unsigned hwid = (wid + 3) /4;
      char*rbuf = (char*) need_result_buf(hwid+1, RBUF_VAL);
      rbuf[hwid] = 0;

      unsigned hval = 0;
      for (unsigned idx = 0; idx < wid ; idx += 1) {
	    unsigned tmp = 0;
	    switch (val.value(idx)) {
		case BIT4_0:
		  tmp = 0;
		  break;
		case BIT4_1:
		  tmp = 1;
		  break;
		case BIT4_X:
		  tmp = 2;
		  break;
		case BIT4_Z:
		  tmp = 3;
		  break;
	    }
	    hval = hval | (tmp << 2*(idx%4));

	    if (idx%4 == 3) {
		  hwid -= 1;
		  rbuf[hwid] = hex_digits[hval];
		  hval = 0;
	    }
      }

      if (hwid > 0) {
	    hwid -= 1;
	    rbuf[hwid] = hex_digits[hval];
	    hval = 0;
      }
      vp->value.str = rbuf;
}

void __vpiVThrVec4Stack::vpi_get_value_int_(p_vpi_value vp, const vvp_vector4_t&val)
{
      int32_t vali = 0;
      int signed_flag = 0;
      vector4_to_value(val, vali, signed_flag, false);
      vp->value.integer = vali;
}

void __vpiVThrVec4Stack::vpi_get_value_real_(p_vpi_value vp, const vvp_vector4_t&val)
{
      unsigned wid = val.size();
      vp->value.real = 0.0;

      for (unsigned idx = wid ; idx > 0 ; idx -= 1) {
	    vp->value.real *= 2.0;
	    if (val.value(idx-1) == BIT4_1)
		  vp->value.real += 1.0;
      }
}

void __vpiVThrVec4Stack::vpi_get_value_string_(p_vpi_value vp, const vvp_vector4_t&val)
{
      char*rbuf = (char*) need_result_buf((val.size() / 8) + 1, RBUF_VAL);
      char*cp = rbuf;

      char tmp = 0;
      for (int bitnr = val.size()-1 ; bitnr >= 0 ; bitnr -= 1) {
	    tmp <<= 1;
	    switch (val.value(bitnr)) {
		case BIT4_1:
		  tmp |= 1;
		  break;
		case BIT4_0:
		default:
		  break;
	    }

	    if ((bitnr&7)==0) {
		    // Don't include leading nuls
		  if (tmp == 0 && cp == rbuf)
			continue;

		  *cp++ = tmp? tmp : ' ';
		  tmp = 0;
	    }
      }
      *cp++ = 0;
      vp->format = vpiStringVal;
      vp->value.str = rbuf;
}

void __vpiVThrVec4Stack::vpi_get_value_vector_(p_vpi_value vp, const vvp_vector4_t&val)
{
      unsigned wid = val.size();

      vp->value.vector = (s_vpi_vecval*)
	    need_result_buf((wid+31)/32*sizeof(s_vpi_vecval), RBUF_VAL);
      assert(vp->value.vector);

      for (unsigned idx = 0 ;  idx < wid ;  idx += 1) {
	    int word = idx/32;
	    PLI_INT32 mask = 1 << (idx%32);

	    switch (val.value(idx)) {
		case BIT4_0:
		  vp->value.vector[word].aval &= ~mask;
		  vp->value.vector[word].bval &= ~mask;
		  break;
		case BIT4_1:
		  vp->value.vector[word].aval |=  mask;
		  vp->value.vector[word].bval &= ~mask;
		  break;
		case BIT4_X:
		  vp->value.vector[word].aval |=  mask;
		  vp->value.vector[word].bval |=  mask;
		  break;
		case BIT4_Z:
		  vp->value.vector[word].aval &= ~mask;
		  vp->value.vector[word].bval |=  mask;
		  break;
	    }
      }
}

vpiHandle __vpiVThrVec4Stack::vpi_put_value(p_vpi_value vp, int /*flags*/)
{
      assert(vpip_current_vthread);

      switch (vp->format) {

	  default:
	    fprintf(stderr, "internal error: vpi_put_value(<format=%d>)"
		    " not implemented for __vpiVThrVec4Stack.\n", vp->format);
	    assert(0);
	    return 0;
      }
}

vpiHandle vpip_make_vthr_str_stack(unsigned depth)
{
      class __vpiVThrStrStack*obj = new __vpiVThrStrStack(depth);
      return obj;
}

vpiHandle vpip_make_vthr_vec4_stack(unsigned depth, bool signed_flag, unsigned wid)
{
      class __vpiVThrVec4Stack*obj = new __vpiVThrVec4Stack(depth, signed_flag, wid);
      return obj;
}

#ifdef CHECK_WITH_VALGRIND
static map<vpiHandle, bool> stack_map;

void thread_string_delete(vpiHandle item)
{
      stack_map[item] = false;
}

static void thread_string_delete_real(vpiHandle item)
{
      class __vpiVThrStrStack*obj = dynamic_cast<__vpiVThrStrStack*>(item);
      delete obj;
}

void vpi_stack_delete()
{
      map<vpiHandle, bool>::iterator iter;
      for (iter = stack_map.begin(); iter != stack_map.end(); ++ iter ) {
	    thread_string_delete_real(iter->first);
      }
}
#endif
