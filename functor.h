#ifndef __functor_H
#define __functor_H
/*
 * Copyright (c) 1999 Stephen Williams (steve@icarus.com)
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
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#if !defined(WINNT)
#ident "$Id: functor.h,v 1.2 1999/11/01 02:07:40 steve Exp $"
#endif

/*
 * The functor is an object that can be applied to a design to
 * transform it. This is different from the target_t, which can only
 * scan the design but not transform it in any way.
 */

class Design;
class NetNet;
class NetProcTop;

struct functor_t {
      virtual ~functor_t();

	/* Signals are scanned first. This is called once for each
	   signal in the design. */
      virtual void signal(class Design*des, class NetNet*);

	/* This method is called for each process in the design. */
      virtual void process(class Design*des, class NetProcTop*);

	/* This method is called for each FF in the design. */
      virtual void lpm_ff(class Design*des, class NetFF*);
};

/*
 * $Log: functor.h,v $
 * Revision 1.2  1999/11/01 02:07:40  steve
 *  Add the synth functor to do generic synthesis
 *  and add the LPM_FF device to handle rows of
 *  flip-flops.
 *
 * Revision 1.1  1999/07/17 22:01:13  steve
 *  Add the functor interface for functor transforms.
 *
 */
#endif
