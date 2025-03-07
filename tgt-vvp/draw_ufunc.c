/*
 * Copyright (c) 2005-2011 Stephen Williams (steve@icarus.com)
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

# include  "vvp_priv.h"
# include  <string.h>
# include  <stdlib.h>
# include  <assert.h>

static void function_argument_logic(ivl_signal_t port, ivl_expr_t expr)
{
      unsigned ewidth, pwidth;

	/* ports cannot be arrays. */
      assert(ivl_signal_dimensions(port) == 0);

      ewidth = ivl_expr_width(expr);
      pwidth = ivl_signal_width(port);

      draw_eval_vec4(expr);
      if (ewidth < pwidth)
	    fprintf(vvp_out, "    %%pad/u %u;\n", pwidth);

      fprintf(vvp_out, "    %%store/vec4 v%p_0, 0, %u;\n", port, pwidth);
}

static void function_argument_real(ivl_signal_t port, ivl_expr_t expr)
{
	/* ports cannot be arrays. */
      assert(ivl_signal_dimensions(port) == 0);

      draw_eval_real(expr);
      fprintf(vvp_out, "    %%store/real v%p_0;\n", port);
}

static void function_argument_bool(ivl_signal_t port, ivl_expr_t expr)
{
	/* For now, treat bit2 variables as bit4 variables. */
      function_argument_logic(port, expr);
}

static void function_argument_class(ivl_signal_t port, ivl_expr_t expr)
{
      draw_eval_object(expr);
      fprintf(vvp_out, "    %%store/obj v%p_0;\n", port);
}

static void function_argument_darray(ivl_signal_t port, ivl_expr_t expr)
{
      draw_eval_object(expr);
      fprintf(vvp_out, "    %%store/obj v%p_0;\n", port);
}

static void function_argument_string(ivl_signal_t port, ivl_expr_t expr)
{
      draw_eval_string(expr);
      fprintf(vvp_out, "    %%store/str v%p_0;\n", port);
}

static void draw_function_argument(ivl_signal_t port, ivl_expr_t expr)
{
      ivl_variable_type_t dtype = ivl_signal_data_type(port);
      switch (dtype) {
	  case IVL_VT_LOGIC:
	    function_argument_logic(port, expr);
	    break;
	  case IVL_VT_REAL:
	    function_argument_real(port, expr);
	    break;
	  case IVL_VT_BOOL:
	    function_argument_bool(port, expr);
	    break;
	  case IVL_VT_CLASS:
	    function_argument_class(port, expr);
	    break;
	  case IVL_VT_STRING:
	    function_argument_string(port, expr);
	    break;
	  case IVL_VT_DARRAY:
	    function_argument_darray(port, expr);
	    break;
	  default:
	    fprintf(stderr, "XXXX function argument %s type=%d?!\n",
		    ivl_signal_basename(port), dtype);
	    assert(0);
      }
}

static void draw_ufunc_preamble(ivl_expr_t expr)
{
      ivl_scope_t def = ivl_expr_def(expr);
      unsigned idx;

        /* If this is an automatic function, allocate the local storage. */
      if (ivl_scope_is_auto(def)) {
            fprintf(vvp_out, "    %%alloc S_%p;\n", def);
      }

	/* evaluate the expressions and send the results to the
	   function ports. */

      assert(ivl_expr_parms(expr) == (ivl_scope_ports(def)-1));
      for (idx = 0 ;  idx < ivl_expr_parms(expr) ;  idx += 1) {
	    ivl_signal_t port = ivl_scope_port(def, idx+1);
	    draw_function_argument(port, ivl_expr_parm(expr, idx));
      }

	/* Call the function */
      fprintf(vvp_out, "    %%fork TD_%s", vvp_mangle_id(ivl_scope_name(def)));
      fprintf(vvp_out, ", S_%p;\n", def);
      fprintf(vvp_out, "    %%join;\n");

}

static void draw_ufunc_epilogue(ivl_expr_t expr)
{
      ivl_scope_t def = ivl_expr_def(expr);

        /* If this is an automatic function, free the local storage. */
      if (ivl_scope_is_auto(def)) {
            fprintf(vvp_out, "    %%free S_%p;\n", def);
      }
}

/*
 * A call to a user defined function generates a result that is the
 * result of this expression.
 *
 * The result of the function is placed by the function execution into
 * a signal within the scope of the function that also has a basename
 * the same as the function. The ivl_target API handled the result
 * mapping already, and we get the name of the result signal as
 * parameter 0 of the function definition.
 */

void draw_ufunc_vec4(ivl_expr_t expr)
{
      ivl_scope_t def = ivl_expr_def(expr);
      ivl_signal_t retval = ivl_scope_port(def, 0);

	/* Take in arguments to function and call function code. */
      draw_ufunc_preamble(expr);

      assert(ivl_signal_dimensions(retval) == 0);
      fprintf(vvp_out, "    %%load/vec4  v%p_0;\n", retval);

      draw_ufunc_epilogue(expr);
}

void draw_ufunc_real(ivl_expr_t expr)
{
      ivl_scope_t def = ivl_expr_def(expr);
      ivl_signal_t retval = ivl_scope_port(def, 0);

	/* Take in arguments to function and call the function code. */
      draw_ufunc_preamble(expr);

	/* Return value signal cannot be an array. */
      assert(ivl_signal_dimensions(retval) == 0);

	/* Load the result into a word. */
      fprintf(vvp_out, "  %%load/real v%p_0;\n", retval);

      draw_ufunc_epilogue(expr);
}

void draw_ufunc_string(ivl_expr_t expr)
{
      ivl_scope_t def = ivl_expr_def(expr);
      ivl_signal_t retval = ivl_scope_port(def, 0);

	/* Take in arguments to function and call the function code. */
      draw_ufunc_preamble(expr);

	/* Return value signal cannot be an array. */
      assert(ivl_signal_dimensions(retval) == 0);

	/* Load the result into a word. */
      fprintf(vvp_out, "  %%load/str v%p_0;\n", retval);

      draw_ufunc_epilogue(expr);
}

void draw_ufunc_object(ivl_expr_t expr)
{
      ivl_scope_t def = ivl_expr_def(expr);
      ivl_signal_t retval = ivl_scope_port(def, 0);

	/* Take in arguments to function and call the function code. */
      draw_ufunc_preamble(expr);

	/* Load the result into the object stack. */
      fprintf(vvp_out, "    %%load/obj v%p_0;\n", retval);

      draw_ufunc_epilogue(expr);
}
