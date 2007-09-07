/*
 * Copyright (c) 2000-2005 Stephen Williams (steve@icarus.com)
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
#ifdef HAVE_CVS_IDENT
#ident "$Id: stub.c,v 1.148 2007/06/12 02:23:40 steve Exp $"
#endif

/*
 * This is a sample target module. All this does is write to the
 * output file some information about each object handle when each of
 * the various object functions is called. This can be used to
 * understand the behavior of the core as it uses a target module.
 */

# include "config.h"
# include "priv.h"
# include <stdlib.h>
# include <inttypes.h>
# include <assert.h>

FILE*out;
int stub_errors = 0;

static struct udp_define_cell {
      ivl_udp_t udp;
      unsigned ref;
      struct udp_define_cell*next;
}*udp_define_list = 0;

static void reference_udp_definition(ivl_udp_t udp)
{
      struct udp_define_cell*cur;

      if (udp_define_list == 0) {
	    udp_define_list = calloc(1, sizeof(struct udp_define_cell));
	    udp_define_list->udp = udp;
	    udp_define_list->ref = 1;
	    return;
      }

      cur = udp_define_list;
      while (cur->udp != udp) {
	    if (cur->next == 0) {
		  cur->next = calloc(1, sizeof(struct udp_define_cell));
		  cur->next->udp = udp;
		  cur->next->ref = 1;
		  return;
	    }

	    cur = cur->next;
      }

      cur->ref += 1;
}

/*
 * This function finds the vector width of a signal. It relies on the
 * assumption that all the signal inputs to the nexus have the same
 * width. The ivl_target API should assert that condition.
 */
unsigned width_of_nexus(ivl_nexus_t nex)
{
      unsigned idx;

      for (idx = 0 ;  idx < ivl_nexus_ptrs(nex) ;  idx += 1) {
	    ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nex, idx);
	    ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

	    if (sig != 0) {
		  return ivl_signal_width(sig);
	    }
      }

	/* ERROR: A nexus should have at least one signal to carry
	   properties like width. */
      return 0;
}

ivl_variable_type_t type_of_nexus(ivl_nexus_t net)
{
      unsigned idx;

      for (idx = 0 ;  idx < ivl_nexus_ptrs(net); idx += 1) {
	    ivl_nexus_ptr_t ptr = ivl_nexus_ptr(net, idx);
	    ivl_signal_t sig = ivl_nexus_ptr_sig(ptr);

	    if (sig != 0) {
		  return ivl_signal_data_type(sig);
	    }
      }

	/* ERROR: A nexus should have at least one signal to carry
	   properties like the data type. */
      return IVL_VT_NO_TYPE;
}

const char*data_type_string(ivl_variable_type_t vtype)
{
      const char*vt = "??";

      switch (vtype) {
	  case IVL_VT_NO_TYPE:
	    vt = "NO_TYPE";
	    break;
	  case IVL_VT_VOID:
	    vt = "void";
	    break;
	  case IVL_VT_BOOL:
	    vt = "bool";
	    break;
	  case IVL_VT_REAL:
	    vt = "real";
	    break;
	  case IVL_VT_LOGIC:
	    vt = "logic";
	    break;
      }

      return vt;
}

/*
 * The compare-like LPM nodes have input widths that match the
 * ivl_lpm_width() value, and an output width of 1. This function
 * checks that that is so, and indicates errors otherwise.
 */
static void check_cmp_widths(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

	/* Check that the input widths are as expected. The inputs
	   must be the width of the ivl_lpm_width() for this device,
	   even though the output for this device is 1 bit. */
      if (width != width_of_nexus(ivl_lpm_data(net,0))) {
	    fprintf(out, "    ERROR: Width of A is %u, not %u\n",
		    width_of_nexus(ivl_lpm_data(net,0)), width);
	    stub_errors += 1;
      }

      if (width != width_of_nexus(ivl_lpm_data(net,1))) {
	    fprintf(out, "    ERROR: Width of B is %u, not %u\n",
		    width_of_nexus(ivl_lpm_data(net,1)), width);
	    stub_errors += 1;
      }

      if (width_of_nexus(ivl_lpm_q(net,0)) != 1) {
	    fprintf(out, "    ERROR: Width of Q is %u, not 1\n",
		    width_of_nexus(ivl_lpm_q(net,0)));
	    stub_errors += 1;
      }
}

static void show_lpm_arithmetic_pins(ivl_lpm_t net)
{
      ivl_nexus_t nex;
      nex = ivl_lpm_q(net, 0);
      fprintf(out, "    Q: %s\n", ivl_nexus_name(ivl_lpm_q(net, 0)));

      nex = ivl_lpm_data(net, 0);
      fprintf(out, "    DataA: %s\n", nex? ivl_nexus_name(nex) : "");

      nex = ivl_lpm_data(net, 1);
      fprintf(out, "    DataB: %s\n", nex? ivl_nexus_name(nex) : "");
}

static void show_lpm_add(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_ADD %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      show_lpm_arithmetic_pins(net);
}

static void show_lpm_array(ivl_lpm_t net)
{
      ivl_nexus_t nex;
      unsigned width = ivl_lpm_width(net);
      ivl_signal_t array = ivl_lpm_array(net);

      fprintf(out, "  LPM_ARRAY: <width=%u, signal=%s>\n",
	      width, ivl_signal_basename(array));
      nex = ivl_lpm_q(net, 0);
      assert(nex);
      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));
      nex = ivl_lpm_select(net);
      assert(nex);
      fprintf(out, "    Address: %s (address width=%u)\n",
	      ivl_nexus_name(nex), ivl_lpm_selects(net));

      if (width_of_nexus(ivl_lpm_q(net,0)) != width) {
	    fprintf(out, "    ERROR: Data Q width doesn't match "
		    "nexus width=%u\n", width_of_nexus(ivl_lpm_q(net,0)));
	    stub_errors += 1;
      }

      if (ivl_signal_width(array) != width) {
	    fprintf(out, "    ERROR: Data  width doesn't match "
		    "word width=%u\n", ivl_signal_width(array));
	    stub_errors += 1;
      }
}

static void show_lpm_divide(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_DIVIDE %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      show_lpm_arithmetic_pins(net);
}

/* IVL_LPM_CMP_EEQ/NEE
 * This LPM node supports two-input compare. The output width is
 * actually always 1, the lpm_width is the expected width of the inputs.
 */
static void show_lpm_cmp_eeq(ivl_lpm_t net)
{
      const char*str = (ivl_lpm_type(net) == IVL_LPM_CMP_EEQ)? "EEQ" : "NEE";
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_CMP_%s %s: <width=%u>\n", str,
	      ivl_lpm_basename(net), width);

      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));
      fprintf(out, "    A: %s\n", ivl_nexus_name(ivl_lpm_data(net,0)));
      fprintf(out, "    B: %s\n", ivl_nexus_name(ivl_lpm_data(net,1)));
      check_cmp_widths(net);
}

/* IVL_LPM_CMP_GE
 * This LPM node supports two-input compare.
 */
static void show_lpm_cmp_ge(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_CMP_GE %s: <width=%u %s>\n",
	      ivl_lpm_basename(net), width,
	      ivl_lpm_signed(net)? "signed" : "unsigned");

      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));
      fprintf(out, "    A: %s\n", ivl_nexus_name(ivl_lpm_data(net,0)));
      fprintf(out, "    B: %s\n", ivl_nexus_name(ivl_lpm_data(net,1)));
      check_cmp_widths(net);
}

/* IVL_LPM_CMP_GT
 * This LPM node supports two-input compare.
 */
static void show_lpm_cmp_gt(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_CMP_GT %s: <width=%u %s>\n",
	      ivl_lpm_basename(net), width,
	      ivl_lpm_signed(net)? "signed" : "unsigned");

      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));
      fprintf(out, "    A: %s\n", ivl_nexus_name(ivl_lpm_data(net,0)));
      fprintf(out, "    B: %s\n", ivl_nexus_name(ivl_lpm_data(net,1)));
      check_cmp_widths(net);
}

/* IVL_LPM_CMP_NE
 * This LPM node supports two-input compare. The output width is
 * actually always 1, the lpm_width is the expected width of the inputs.
 */
static void show_lpm_cmp_ne(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_CMP_NE %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));
      fprintf(out, "    A: %s\n", ivl_nexus_name(ivl_lpm_data(net,0)));
      fprintf(out, "    B: %s\n", ivl_nexus_name(ivl_lpm_data(net,1)));
      check_cmp_widths(net);
}

/* IVL_LPM_CONCAT
 * The concat device takes N inputs (N=ivl_lpm_selects) and generates
 * a single output. The total output is known from the ivl_lpm_width
 * function. The widths of all the inputs are inferred from the widths
 * of the signals connected to the nexus of the inputs. The compiler
 * makes sure the input widths add up to the output width.
 */
static void show_lpm_concat(ivl_lpm_t net)
{
      unsigned idx;

      unsigned width_sum = 0;
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_CONCAT %s: <width=%u, inputs=%u>\n",
	      ivl_lpm_basename(net), width, ivl_lpm_selects(net));
      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));

      for (idx = 0 ;  idx < ivl_lpm_selects(net) ;  idx += 1) {
	    ivl_nexus_t nex = ivl_lpm_data(net, idx);
	    unsigned signal_width = width_of_nexus(nex);

	    fprintf(out, "    I%u: %s (width=%u)\n", idx,
		    ivl_nexus_name(nex), signal_width);
	    width_sum += signal_width;
      }

      if (width_sum != width) {
	    fprintf(out, "    ERROR! Got %u bits input, expecting %u!\n",
		    width_sum, width);
      }
}

static void show_lpm_ff(ivl_lpm_t net)
{
      ivl_nexus_t nex;
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_FF %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      nex = ivl_lpm_clk(net);
      fprintf(out, "    clk: %s\n", ivl_nexus_name(nex));
      if (width_of_nexus(nex) != 1) {
	    fprintf(out, "    clk: ERROR: Nexus width is %u\n",
		    width_of_nexus(nex));
	    stub_errors += 1;
      }

      if (ivl_lpm_enable(net)) {
	    nex = ivl_lpm_enable(net);
	    fprintf(out, "    CE: %s\n", ivl_nexus_name(nex));
	    if (width_of_nexus(nex) != 1) {
		  fprintf(out, "    CE: ERROR: Nexus width is %u\n",
			  width_of_nexus(nex));
		  stub_errors += 1;
	    }
      }

      nex = ivl_lpm_data(net,0);
      fprintf(out, "    D: %s\n", ivl_nexus_name(nex));
      if (width_of_nexus(nex) != width) {
	    fprintf(out, "    D: ERROR: Nexus width is %u\n",
		    width_of_nexus(nex));
	    stub_errors += 1;
      }

      nex = ivl_lpm_q(net,0);
      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));
      if (width_of_nexus(nex) != width) {
	    fprintf(out, "    Q: ERROR: Nexus width is %u\n",
		    width_of_nexus(nex));
	    stub_errors += 1;
      }

}

static void show_lpm_mod(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_MOD %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      show_lpm_arithmetic_pins(net);
}

/*
 * The LPM_MULT node has a Q output and two data inputs. The width of
 * the Q output must be the width of the node itself.
 */
static void show_lpm_mult(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_MULT %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));
      fprintf(out, "    A: %s <width=%u>\n",
	      ivl_nexus_name(ivl_lpm_data(net,0)),
	      width_of_nexus(ivl_lpm_data(net,0)));
      fprintf(out, "    B: %s <width=%u>\n",
	      ivl_nexus_name(ivl_lpm_data(net,1)),
	      width_of_nexus(ivl_lpm_data(net,1)));

      if (width != width_of_nexus(ivl_lpm_q(net,0))) {
	    fprintf(out, "    ERROR: Width of Q is %u, not %u\n",
		    width_of_nexus(ivl_lpm_q(net,0)), width);
	    stub_errors += 1;
      }
}

/*
 * Show an IVL_LPM_MUX.
 *
 * The compiler is supposed to make sure that the Q output and data
 * inputs all have the width of the device. The ivl_lpm_select input
 * has its own width.
 */
static void show_lpm_mux(ivl_lpm_t net)
{
      ivl_nexus_t nex;
      unsigned idx;
      unsigned width = ivl_lpm_width(net);
      unsigned size  = ivl_lpm_size(net);

      fprintf(out, "  LPM_MUX %s: <width=%u, size=%u>\n",
	      ivl_lpm_basename(net), width, size);

      nex = ivl_lpm_q(net,0);
      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));
      if (width != width_of_nexus(nex)) {
	    fprintf(out, "    Q: ERROR: Nexus width is %u\n",
		    width_of_nexus(nex));
	    stub_errors += 1;
      }

	/* The select input is a vector with the width from the
	   ivl_lpm_selects function. */
      nex = ivl_lpm_select(net);
      fprintf(out, "    S: %s <width=%u>\n",
	      ivl_nexus_name(nex),
	      ivl_lpm_selects(net));
      if (ivl_lpm_selects(net) != width_of_nexus(nex)) {
	    fprintf(out, "    S: ERROR: Nexus width is %u\n",
		    width_of_nexus(nex));
	    stub_errors += 1;
      }

	/* The ivl_lpm_size() method give the number of inputs that
	   can be selected from. */
      for (idx = 0 ;  idx < size ;  idx += 1) {
	    nex = ivl_lpm_data(net,idx);
	    fprintf(out, "    D%u: %s\n", idx, ivl_nexus_name(nex));
	    if (width != width_of_nexus(nex)) {
		  fprintf(out, "    D%u: ERROR, Nexus width is %u\n",
			  idx, width_of_nexus(nex));
		  stub_errors += 1;
	    }
      }
}

static void show_lpm_part(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);
      unsigned base  = ivl_lpm_base(net);
      ivl_nexus_t sel = ivl_lpm_data(net,1);
      const char*part_type_string = "";

      switch (ivl_lpm_type(net)) {
	  case IVL_LPM_PART_VP:
	    part_type_string = "VP";
	    break;
	  case IVL_LPM_PART_PV:
	    part_type_string = "PV";
	    break;
	  default:
	    break;
      }

      fprintf(out, "  LPM_PART_%s %s: <width=%u, base=%u, signed=%d>\n",
	      part_type_string, ivl_lpm_basename(net),
	      width, base, ivl_lpm_signed(net));
      fprintf(out, "    O: %s\n", ivl_nexus_name(ivl_lpm_q(net,0)));
      fprintf(out, "    I: %s\n", ivl_nexus_name(ivl_lpm_data(net,0)));

      if (sel != 0) {
	    fprintf(out, "    S: %s\n", ivl_nexus_name(sel));
	    if (base != 0) {
		  fprintf(out, "   ERROR: Part select has base AND selector\n");
		  stub_errors += 1;
	    }
      }

	/* The compiler must assure that the base plus the part select
	   width fits within the input to the part select. */
      switch (ivl_lpm_type(net)) {

	  case IVL_LPM_PART_VP:
	    if (width_of_nexus(ivl_lpm_data(net,0)) < (width+base)) {
		  fprintf(out, "    ERROR: Part select is out of range."
			  " Data nexus width=%u, width+base=%u\n",
			  width_of_nexus(ivl_lpm_data(net,0)), width+base);
		  stub_errors += 1;
	    }

	    if (width_of_nexus(ivl_lpm_q(net,0)) != width) {
		  fprintf(out, "    ERROR: Part select input mistatch."
			  " Nexus width=%u, expect width=%u\n",
			  width_of_nexus(ivl_lpm_q(net,0)), width);
		  stub_errors += 1;
	    }
	    break;

	  case IVL_LPM_PART_PV:
	    if (width_of_nexus(ivl_lpm_q(net,0)) < (width+base)) {
		  fprintf(out, "    ERROR: Part select is out of range."
			  " Target nexus width=%u, width+base=%u\n",
			  width_of_nexus(ivl_lpm_q(net,0)), width+base);
		  stub_errors += 1;
	    }

	    if (width_of_nexus(ivl_lpm_data(net,0)) != width) {
		  fprintf(out, "    ERROR: Part select input mistatch."
			  " Nexus width=%u, expect width=%u\n",
			  width_of_nexus(ivl_lpm_data(net,0)), width);
		  stub_errors += 1;
	    }
	    break;

	  default:
	    assert(0);
      }
}

static void show_lpm_part_bi(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);
      unsigned base  = ivl_lpm_base(net);
      ivl_nexus_t port_p = ivl_lpm_q(net,0);
      ivl_nexus_t port_v = ivl_lpm_data(net,0);

      fprintf(out, "  LPM_PART_BI %s: <width=%u, base=%u, signed=%d>\n",
	      ivl_lpm_basename(net), width, base, ivl_lpm_signed(net));
      fprintf(out, "    P: %s\n", ivl_nexus_name(port_p));
      fprintf(out, "    V: %s <width=%u>\n", ivl_nexus_name(port_v),
	      width_of_nexus(port_v));


	/* The data(0) port must be large enough for the part select. */
      if (width_of_nexus(ivl_lpm_data(net,0)) < (width+base)) {
	    fprintf(out, "    ERROR: Part select is out of range."
		    " Data nexus width=%u, width+base=%u\n",
		    width_of_nexus(ivl_lpm_data(net,0)), width+base);
	    stub_errors += 1;
      }

	/* The Q vector must be exactly the width of the part select. */
      if (width_of_nexus(ivl_lpm_q(net,0)) != width) {
	    fprintf(out, "    ERROR: Part select input mistatch."
		    " Nexus width=%u, expect width=%u\n",
		    width_of_nexus(ivl_lpm_q(net,0)), width);
	    stub_errors += 1;
      }
}


/*
 * The reduction operators have similar characteristics and are
 * displayed here.
 */
static void show_lpm_re(ivl_lpm_t net)
{
      ivl_nexus_t nex;
      const char*type = "?";
      unsigned width = ivl_lpm_width(net);

      switch (ivl_lpm_type(net)) {
	  case IVL_LPM_RE_AND:
	    type = "AND";
	    break;
	  case IVL_LPM_RE_NAND:
	    type = "NAND";
	    break;
	  case IVL_LPM_RE_OR:
	    type = "OR";
	    break;
	  case IVL_LPM_RE_NOR:
	    type = "NOR";
	  case IVL_LPM_RE_XOR:
	    type = "XOR";
	    break;
	  case IVL_LPM_RE_XNOR:
	    type = "XNOR";
	  default:
	    break;
      }

      fprintf(out, "  LPM_RE_%s: %s <width=%u>\n",
	      type, ivl_lpm_name(net),width);

      nex = ivl_lpm_q(net, 0);
      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));

      nex = ivl_lpm_data(net, 0);
      fprintf(out, "    D: %s\n", ivl_nexus_name(nex));

      nex = ivl_lpm_q(net, 0);

      if (1 != width_of_nexus(nex)) {
	    fprintf(out, "    ERROR: Width of Q is %u, expecting 1\n",
		    width_of_nexus(nex));
	    stub_errors += 1;
      }

      nex = ivl_lpm_data(net, 0);
      if (width != width_of_nexus(nex)) {
	    fprintf(out, "    ERROR: Width of input is %u, expecting %u\n",
		    width_of_nexus(nex), width);
	    stub_errors += 1;
      }
}

static void show_lpm_repeat(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);
      unsigned count = ivl_lpm_size(net);
      ivl_nexus_t nex_q = ivl_lpm_q(net,0);
      ivl_nexus_t nex_a = ivl_lpm_data(net,0);

      fprintf(out, "  LPM_REPEAT %s: <width=%u, count=%u>\n",
	      ivl_lpm_basename(net), width, count);

      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex_q));
      fprintf(out, "    D: %s\n", ivl_nexus_name(nex_a));

      if (width != width_of_nexus(nex_q)) {
	    fprintf(out, "    ERROR: Width of Q is %u, expecting %u\n",
		    width_of_nexus(nex_q), width);
	    stub_errors += 1;
      }

      if (count == 0 || count > width || (width%count != 0)) {
	    fprintf(out, "    ERROR: Repeat count not reasonable\n");
	    stub_errors += 1;

      } else if (width/count != width_of_nexus(nex_a)) {
	    fprintf(out, "    ERROR: Windth of D is %u, expecting %u\n",
		    width_of_nexus(nex_a), width/count);
	    stub_errors += 1;
      }
}

static void show_lpm_shift(ivl_lpm_t net, const char*shift_dir)
{
      ivl_nexus_t nex;
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_SHIFT%s %s: <width=%u, %ssigned>\n", shift_dir,
	      ivl_lpm_basename(net), width,
	      ivl_lpm_signed(net)? "" : "un");

      nex = ivl_lpm_q(net, 0);
      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));

      if (width != width_of_nexus(nex)) {
	    fprintf(out, "    ERROR: Q output nexus width=%u "
		    "does not match part width\n", width_of_nexus(nex));
	    stub_errors += 1;
      }

      nex = ivl_lpm_data(net, 0);
      fprintf(out, "    D: %s\n", ivl_nexus_name(nex));

      if (width != width_of_nexus(nex)) {
	    fprintf(out, "    ERROR: Q output nexus width=%u "
		    "does not match part width\n", width_of_nexus(nex));
	    stub_errors += 1;
      }

      nex = ivl_lpm_data(net, 1);
      fprintf(out, "    S: %s <width=%u>\n",
	      ivl_nexus_name(nex), width_of_nexus(nex));
}

static void show_lpm_sign_ext(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);
      ivl_nexus_t nex_q = ivl_lpm_q(net,0);
      ivl_nexus_t nex_a = ivl_lpm_data(net,0);

      fprintf(out, "  LPM_SIGN_EXT %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex_q));
      fprintf(out, "    D: %s <width=%u>\n",
	      ivl_nexus_name(nex_a), width_of_nexus(nex_a));

      if (width != width_of_nexus(nex_q)) {
	    fprintf(out, "    ERROR: Width of Q is %u, expecting %u\n",
		    width_of_nexus(nex_q), width);
	    stub_errors += 1;
      }
}

static void show_lpm_sub(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);

      fprintf(out, "  LPM_SUB %s: <width=%u>\n",
	      ivl_lpm_basename(net), width);

      show_lpm_arithmetic_pins(net);
}

static void show_lpm_sfunc(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);
      unsigned ports = ivl_lpm_size(net);
      ivl_variable_type_t data_type = type_of_nexus(ivl_lpm_q(net,0));
      ivl_nexus_t nex;
      unsigned idx;

      fprintf(out, "  LPM_SFUNC %s: <call=%s, width=%u, type=%s, ports=%u>\n",
	      ivl_lpm_basename(net), ivl_lpm_string(net),
	      width, data_type_string(data_type), ports);

      nex = ivl_lpm_q(net, 0);
      if (width != width_of_nexus(nex)) {
	    fprintf(out, "    ERROR: Q output nexus width=%u "
		    " does not match part width\n", width_of_nexus(nex));
	    stub_errors += 1;
      }

      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));
      for (idx = 0 ;  idx < ports ;  idx += 1) {
	    nex = ivl_lpm_data(net, idx);
	    fprintf(out, "    D%u: %s <width=%u, type=%s>\n", idx,
		    ivl_nexus_name(nex), width_of_nexus(nex),
		    data_type_string(type_of_nexus(nex)));
      }
}

static void show_lpm_ufunc(ivl_lpm_t net)
{
      unsigned width = ivl_lpm_width(net);
      unsigned ports = ivl_lpm_size(net);
      ivl_scope_t def = ivl_lpm_define(net);
      ivl_nexus_t nex;
      unsigned idx;

      fprintf(out, "  LPM_UFUNC %s: <call=%s, width=%u, ports=%u>\n",
	      ivl_lpm_basename(net), ivl_scope_name(def), width, ports);

      nex = ivl_lpm_q(net, 0);
      if (width != width_of_nexus(nex)) {
	    fprintf(out, "    ERROR: Q output nexus width=%u "
		    " does not match part width\n", width_of_nexus(nex));
	    stub_errors += 1;
      }

      fprintf(out, "    Q: %s\n", ivl_nexus_name(nex));

      for (idx = 0 ;  idx < ports ;  idx += 1) {
	    nex = ivl_lpm_data(net, idx);
	    fprintf(out, "    D%u: %s <width=%u>\n", idx,
		    ivl_nexus_name(nex), width_of_nexus(nex));
      }
}

static void show_lpm(ivl_lpm_t net)
{

      switch (ivl_lpm_type(net)) {

	  case IVL_LPM_ADD:
	    show_lpm_add(net);
	    break;

	  case IVL_LPM_ARRAY:
	    show_lpm_array(net);
	    break;

	  case IVL_LPM_DIVIDE:
	    show_lpm_divide(net);
	    break;

	  case IVL_LPM_CMP_EEQ:
	  case IVL_LPM_CMP_NEE:
	    show_lpm_cmp_eeq(net);
	    break;

	  case IVL_LPM_FF:
	    show_lpm_ff(net);
	    break;

	  case IVL_LPM_CMP_GE:
	    show_lpm_cmp_ge(net);
	    break;

	  case IVL_LPM_CMP_GT:
	    show_lpm_cmp_gt(net);
	    break;

	  case IVL_LPM_CMP_NE:
	    show_lpm_cmp_ne(net);
	    break;

	  case IVL_LPM_CONCAT:
	    show_lpm_concat(net);
	    break;

	  case IVL_LPM_RE_AND:
	  case IVL_LPM_RE_NAND:
	  case IVL_LPM_RE_NOR:
	  case IVL_LPM_RE_OR:
	  case IVL_LPM_RE_XOR:
	  case IVL_LPM_RE_XNOR:
	    show_lpm_re(net);
	    break;

	  case IVL_LPM_SHIFTL:
	    show_lpm_shift(net, "L");
	    break;

	  case IVL_LPM_SIGN_EXT:
	    show_lpm_sign_ext(net);
	    break;

	  case IVL_LPM_SHIFTR:
	    show_lpm_shift(net, "R");
	    break;

	  case IVL_LPM_SUB:
	    show_lpm_sub(net);
	    break;

	  case IVL_LPM_MOD:
	    show_lpm_mod(net);
	    break;

	  case IVL_LPM_MULT:
	    show_lpm_mult(net);
	    break;

	  case IVL_LPM_MUX:
	    show_lpm_mux(net);
	    break;

	  case IVL_LPM_PART_VP:
	  case IVL_LPM_PART_PV:
	    show_lpm_part(net);
	    break;

	      /* The BI part select is slightly special. */
	  case IVL_LPM_PART_BI:
	    show_lpm_part_bi(net);
	    break;

	  case IVL_LPM_REPEAT:
	    show_lpm_repeat(net);
	    break;

	  case IVL_LPM_SFUNC:
	    show_lpm_sfunc(net);
	    break;

	  case IVL_LPM_UFUNC:
	    show_lpm_ufunc(net);
	    break;

	  default:
	    fprintf(out, "  LPM(%d) %s: <width=%u, signed=%d>\n",
		    ivl_lpm_type(net),
		    ivl_lpm_basename(net),
		    ivl_lpm_width(net),
		    ivl_lpm_signed(net));
      }
}


static int show_process(ivl_process_t net, void*x)
{
      unsigned idx;

      switch (ivl_process_type(net)) {
	  case IVL_PR_INITIAL:
	    fprintf(out, "initial\n");
	    break;
	  case IVL_PR_ALWAYS:
	    fprintf(out, "always\n");
	    break;
      }

      for (idx = 0 ;  idx < ivl_process_attr_cnt(net) ;  idx += 1) {
	    ivl_attribute_t attr = ivl_process_attr_val(net, idx);
	    switch (attr->type) {
		case IVL_ATT_VOID:
		  fprintf(out, "    (* %s *)\n", attr->key);
		  break;
		case IVL_ATT_STR:
		  fprintf(out, "    (* %s = \"%s\" *)\n", attr->key,
			  attr->val.str);
		  break;
		case IVL_ATT_NUM:
		  fprintf(out, "    (* %s = %ld *)\n", attr->key,
			  attr->val.num);
		  break;
	    }
      }

      show_statement(ivl_process_stmt(net), 4);

      return 0;
}

static void show_parameter(ivl_parameter_t net)
{
      const char*name = ivl_parameter_basename(net);
      fprintf(out, "   parameter %s;\n", name);
      show_expression(ivl_parameter_expr(net), 7);
}

static void show_event(ivl_event_t net)
{
      unsigned idx;
      fprintf(out, "  event %s (%u pos, %u neg, %u any);\n",
	      ivl_event_basename(net), ivl_event_npos(net),
	      ivl_event_nneg(net), ivl_event_nany(net));

      for (idx = 0 ;  idx < ivl_event_nany(net) ;  idx += 1) {
	    ivl_nexus_t nex = ivl_event_any(net, idx);
	    fprintf(out, "      ANYEDGE: %s\n", ivl_nexus_name(nex));
      }

      for (idx = 0 ;  idx < ivl_event_nneg(net) ;  idx += 1) {
	    ivl_nexus_t nex = ivl_event_neg(net, idx);
	    fprintf(out, "      NEGEDGE: %s\n", ivl_nexus_name(nex));
      }

      for (idx = 0 ;  idx < ivl_event_npos(net) ;  idx += 1) {
	    ivl_nexus_t nex = ivl_event_pos(net, idx);
	    fprintf(out, "      POSEDGE: %s\n", ivl_nexus_name(nex));
      }
}

static const char* str_tab[8] = {
      "HiZ", "small", "medium", "weak",
      "large", "pull", "strong", "supply"};

/*
 * This function is used by the show_signal to dump a constant value
 * that is connected to the signal. While we are here, check that the
 * value is consistent with the signal itself.
 */
static void signal_nexus_const(ivl_signal_t sig,
			       ivl_nexus_ptr_t ptr,
			       ivl_net_const_t con)
{
      const char*dr0 = str_tab[ivl_nexus_ptr_drive0(ptr)];
      const char*dr1 = str_tab[ivl_nexus_ptr_drive1(ptr)];

      const char*bits;
      unsigned idx, width = ivl_const_width(con);

      fprintf(out, "      const-");

      switch (ivl_const_type(con)) {
	  case IVL_VT_LOGIC:
	    bits = ivl_const_bits(con);
 	    for (idx = 0 ;  idx < width ;  idx += 1) {
		  fprintf(out, "%c", bits[width-idx-1]);
	    }
	    break;

	  case IVL_VT_REAL:
	    fprintf(out, "%lf", ivl_const_real(con));
	    break;

	  default:
	    fprintf(out, "????");
	    break;
      }

      fprintf(out, " (%s0, %s1, width=%u)\n", dr0, dr1, width);

      if (ivl_signal_width(sig) != width) {
	    fprintf(out, "ERROR: Width of signal does not match "
		    "width of connected constant vector.\n");
	    stub_errors += 1;
      }

      if (ivl_signal_data_type(sig) != ivl_const_type(con)) {
	    fprintf(out, "ERROR: Signal data type does not match"
		    " literal type.\n");
	    stub_errors += 1;
      }
}

static void show_nexus_details(ivl_signal_t net, ivl_nexus_t nex)
{
      unsigned idx;

      for (idx = 0 ;  idx < ivl_nexus_ptrs(nex) ;  idx += 1) {
	    ivl_net_const_t con;
	    ivl_net_logic_t log;
	    ivl_lpm_t lpm;
	    ivl_signal_t sig;
	    ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nex, idx);

	    const char*dr0 = str_tab[ivl_nexus_ptr_drive0(ptr)];
	    const char*dr1 = str_tab[ivl_nexus_ptr_drive1(ptr)];

	    if ((sig = ivl_nexus_ptr_sig(ptr))) {
		  fprintf(out, "      SIG %s word=%u (%s0, %s1)",
			  ivl_signal_name(sig), ivl_nexus_ptr_pin(ptr), dr0, dr1);

		  if (ivl_signal_width(sig) != ivl_signal_width(net)) {
			fprintf(out, " (ERROR: Width=%u)",
				ivl_signal_width(sig));
			stub_errors += 1;
		  }

		  if (ivl_signal_data_type(sig) != ivl_signal_data_type(net)) {
			fprintf(out, " (ERROR: data type mismatch)");
			stub_errors += 1;
		  }

		  fprintf(out, "\n");

	    } else if ((log = ivl_nexus_ptr_log(ptr))) {
		  fprintf(out, "      LOG %s.%s[%u] (%s0, %s1)\n",
			  ivl_scope_name(ivl_logic_scope(log)),
			  ivl_logic_basename(log),
			  ivl_nexus_ptr_pin(ptr), dr0, dr1);

	    } else if ((lpm = ivl_nexus_ptr_lpm(ptr))) {
		  fprintf(out, "      LPM %s.%s (%s0, %s1)\n",
			  ivl_scope_name(ivl_lpm_scope(lpm)),
			  ivl_lpm_basename(lpm), dr0, dr1);

	    } else if ((con = ivl_nexus_ptr_con(ptr))) {
		  signal_nexus_const(net, ptr, con);

	    } else {
		  fprintf(out, "      ?[%u] (%s0, %s1)\n",
			  ivl_nexus_ptr_pin(ptr), dr0, dr1);
	    }
      }
}

static void show_signal(ivl_signal_t net)
{
      unsigned idx;
      ivl_nexus_t nex;

      const char*type = "?";
      const char*port = "";
      const char*data_type = "?";
      const char*sign = ivl_signal_signed(net)? "signed" : "unsigned";

      switch (ivl_signal_type(net)) {
	  case IVL_SIT_REG:
	    type = "reg";
	    break;
	  case IVL_SIT_TRI:
	    type = "tri";
	    break;
	  case IVL_SIT_TRI0:
	    type = "tri0";
	    break;
	  case IVL_SIT_TRI1:
	    type = "tri1";
	    break;
	  default:
	    break;
      }

      switch (ivl_signal_port(net)) {

	  case IVL_SIP_INPUT:
	    port = "input ";
	    break;

	  case IVL_SIP_OUTPUT:
	    port = "output ";
	    break;

	  case IVL_SIP_INOUT:
	    port = "inout ";
	    break;

	  case IVL_SIP_NONE:
	    break;
      }

      switch (ivl_signal_data_type(net)) {

	  case IVL_VT_BOOL:
	    data_type = "bool";
	    break;

	  case IVL_VT_LOGIC:
	    data_type = "logic";
	    break;

	  case IVL_VT_REAL:
	    data_type = "real";
	    break;

	  default:
	    data_type = "?data?";
	    break;
      }

      for (idx = 0 ;  idx < ivl_signal_array_count(net) ; idx += 1) {

	    nex = ivl_signal_nex(net, idx);

	    fprintf(out, "  %s %s %s%s[%d:%d] %s[word=%u, adr=%d]  <width=%u> nexus=%s\n",
		    type, sign, port, data_type,
		    ivl_signal_msb(net), ivl_signal_lsb(net),
		    ivl_signal_basename(net),
		    idx, ivl_signal_array_base(net)+idx,
		    ivl_signal_width(net),
		    ivl_nexus_name(nex));

	    show_nexus_details(net, nex);
      }

      for (idx = 0 ;  idx < ivl_signal_npath(net) ;  idx += 1) {
	    ivl_delaypath_t path = ivl_signal_path(net,idx);
	    ivl_nexus_t nex = ivl_path_source(path);
	    ivl_nexus_t con = ivl_path_condit(path);
	    int posedge = ivl_path_source_posedge(path);
	    int negedge = ivl_path_source_negedge(path);

	    fprintf(out, "      path %s", ivl_nexus_name(nex));
	    if (posedge) fprintf(out, " posedge");
	    if (negedge) fprintf(out, " negedge");
	    if (con) fprintf(out, " (if %s)", ivl_nexus_name(con));
	    fprintf(out, " %" PRIu64 ",%" PRIu64 ",%" PRIu64
		         " %" PRIu64 ",%" PRIu64 ",%" PRIu64
		         " %" PRIu64 ",%" PRIu64 ",%" PRIu64
		         " %" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
		    ivl_path_delay(path, IVL_PE_01),
		    ivl_path_delay(path, IVL_PE_10),
		    ivl_path_delay(path, IVL_PE_0z),
		    ivl_path_delay(path, IVL_PE_z1),
		    ivl_path_delay(path, IVL_PE_1z),
		    ivl_path_delay(path, IVL_PE_z0),
		    ivl_path_delay(path, IVL_PE_0x),
		    ivl_path_delay(path, IVL_PE_x1),
		    ivl_path_delay(path, IVL_PE_1x),
		    ivl_path_delay(path, IVL_PE_x0),
		    ivl_path_delay(path, IVL_PE_xz),
		    ivl_path_delay(path, IVL_PE_zx));
      }

      for (idx = 0 ;  idx < ivl_signal_attr_cnt(net) ;  idx += 1) {
	    ivl_attribute_t atr = ivl_signal_attr_val(net, idx);

	    switch (atr->type) {
		case IVL_ATT_STR:
		  fprintf(out, "    %s = %s\n", atr->key, atr->val.str);
		  break;
		case IVL_ATT_NUM:
		  fprintf(out, "    %s = %ld\n", atr->key, atr->val.num);
		  break;
		case IVL_ATT_VOID:
		  fprintf(out, "    %s\n", atr->key);
		  break;
	    }
      }

}

static void test_expr_is_delay(ivl_expr_t exp)
{
      switch (ivl_expr_type(exp)) {
	  case IVL_EX_ULONG:
	    return;
	  case IVL_EX_NUMBER:
	    return;
	  case IVL_EX_SIGNAL:
	    return;
	  default:
	    break;
      }

      fprintf(out, "      ERROR: Expression is not a suitable delay\n");
      stub_errors += 1;
}

/*
 * All logic gates have inputs and outputs that match exactly in
 * width. For example, and AND gate with 4 bit inputs generates a 4
 * bit output, and all the inputs are 4 bits.
 */
static void show_logic(ivl_net_logic_t net)
{
      unsigned npins, idx;
      const char*name = ivl_logic_basename(net);
      ivl_drive_t drive0 = ivl_logic_drive0(net);
      ivl_drive_t drive1 = ivl_logic_drive1(net);

      switch (ivl_logic_type(net)) {
	  case IVL_LO_AND:
	    fprintf(out, "  and %s", name);
	    break;
	  case IVL_LO_BUF:
	    fprintf(out, "  buf %s", name);
	    break;
	  case IVL_LO_BUFIF0:
	    fprintf(out, "  bufif0 %s", name);
	    break;
	  case IVL_LO_BUFIF1:
	    fprintf(out, "  bufif1 %s", name);
	    break;
	  case IVL_LO_BUFZ:
	    fprintf(out, "  bufz %s", name);
	    break;
	  case IVL_LO_CMOS:
	    fprintf(out, "  cmos %s", name);
	    break;
	  case IVL_LO_NAND:
	    fprintf(out, "  nand %s", name);
	    break;
	  case IVL_LO_NMOS:
	    fprintf(out, "  nmos %s", name);
	    break;
	  case IVL_LO_NOR:
	    fprintf(out, "  nor %s", name);
	    break;
	  case IVL_LO_NOT:
	    fprintf(out, "  not %s", name);
	    break;
	  case IVL_LO_NOTIF0:
	    fprintf(out, "  notif0 %s", name);
	    break;
	  case IVL_LO_NOTIF1:
	    fprintf(out, "  notif1 %s", name);
	    break;
	  case IVL_LO_OR:
	    fprintf(out, "  or %s", name);
	    break;
	  case IVL_LO_PMOS:
	    fprintf(out, "  pmos %s", name);
	    break;
	  case IVL_LO_PULLDOWN:
	    fprintf(out, "  pulldown %s", name);
	    break;
	  case IVL_LO_PULLUP:
	    fprintf(out, "  pullup %s", name);
	    break;
	  case IVL_LO_RCMOS:
	    fprintf(out, "  rcmos %s", name);
	    break;
	  case IVL_LO_RNMOS:
	    fprintf(out, "  rnmos %s", name);
	    break;
	  case IVL_LO_RPMOS:
	    fprintf(out, "  rpmos %s", name);
	    break;
	  case IVL_LO_XNOR:
	    fprintf(out, "  xnor %s", name);
	    break;
	  case IVL_LO_XOR:
	    fprintf(out, "  xor %s", name);
	    break;

	  case IVL_LO_UDP:
	    fprintf(out, "  primitive<%s> %s",
		    ivl_udp_name(ivl_logic_udp(net)), name);
	    break;

	  default:
	    fprintf(out, "  unsupported gate<type=%d> %s", ivl_logic_type(net), name);
	    break;
      }

      fprintf(out, " <width=%u>\n", ivl_logic_width(net));

      fprintf(out, "    <Delays...>\n");
      if (ivl_logic_delay(net,0)) {
	    test_expr_is_delay(ivl_logic_delay(net,0));
	    show_expression(ivl_logic_delay(net,0), 6);
      }
      if (ivl_logic_delay(net,1)) {
	    test_expr_is_delay(ivl_logic_delay(net,1));
	    show_expression(ivl_logic_delay(net,1), 6);
      }
      if (ivl_logic_delay(net,2)) {
	    test_expr_is_delay(ivl_logic_delay(net,2));
	    show_expression(ivl_logic_delay(net,2), 6);
      }

      npins = ivl_logic_pins(net);

	/* Show the pins of the gate. Pin-0 is always the output, and
	   the remaining pins are the inputs. Inputs may be
	   unconnected, but if connected the nexus width must exactly
	   match the gate width. */
      for (idx = 0 ;  idx < npins ;  idx += 1) {
	    ivl_nexus_t nex = ivl_logic_pin(net, idx);
	    const char*nexus_name =  nex? ivl_nexus_name(nex) : "";

	    fprintf(out, "    %d: %s", idx, nexus_name);
	    if (idx == 0)
		  fprintf(out, " <drive0/1 = %u/%u>", drive0, drive1);
	    fprintf(out, "\n");

	    if (nex == 0) {
		  if (idx == 0) {
			fprintf(out, "    0: ERROR: Pin 0 must not "
				"be unconnected\n");
			stub_errors += 1;
		  }
		  continue;
	    }

	    if (ivl_logic_width(net) != width_of_nexus(nex)) {
		  fprintf(out, "    %d: ERROR: Nexus width is %u\n",
			  idx, width_of_nexus(nex));
		  stub_errors += 1;
	    }
      }

	/* If this is an instance of a UDP, then check that the
	   instantiation is consistent with the definition. */
      if (ivl_logic_type(net) == IVL_LO_UDP) {
	    ivl_udp_t udp = ivl_logic_udp(net);
	    if (npins != 1+ivl_udp_nin(udp)) {
		  fprintf(out, "    ERROR: UDP %s expects %u inputs\n",
			  ivl_udp_name(udp), ivl_udp_nin(udp));
		  stub_errors += 1;
	    }

	      /* Add a reference to this udp definition. */
	    reference_udp_definition(udp);
      }

      npins = ivl_logic_attr_cnt(net);
      for (idx = 0 ;  idx < npins ;  idx += 1) {
	    ivl_attribute_t cur = ivl_logic_attr_val(net,idx);
	    switch (cur->type) {
		case IVL_ATT_VOID:
		  fprintf(out, "    %s\n", cur->key);
		  break;
		case IVL_ATT_NUM:
		  fprintf(out, "    %s = %ld\n", cur->key, cur->val.num);
		  break;
		case IVL_ATT_STR:
		  fprintf(out, "    %s = %s\n", cur->key, cur->val.str);
		  break;
	    }
      }
}

static int show_scope(ivl_scope_t net, void*x)
{
      unsigned idx;

      fprintf(out, "scope: %s (%u parameters, %u signals, %u logic)",
	      ivl_scope_name(net), ivl_scope_params(net),
	      ivl_scope_sigs(net), ivl_scope_logs(net));

      switch (ivl_scope_type(net)) {
	  case IVL_SCT_MODULE:
	    fprintf(out, " module %s", ivl_scope_tname(net));
	    break;
	  case IVL_SCT_FUNCTION:
	    fprintf(out, " function %s", ivl_scope_tname(net));
	    break;
	  case IVL_SCT_BEGIN:
	    fprintf(out, " begin : %s", ivl_scope_tname(net));
	    break;
	  case IVL_SCT_FORK:
	    fprintf(out, " fork : %s", ivl_scope_tname(net));
	    break;
	  case IVL_SCT_TASK:
	    fprintf(out, " task %s", ivl_scope_tname(net));
	    break;
	  default:
	    fprintf(out, " type(%u) %s", ivl_scope_type(net),
		    ivl_scope_tname(net));
	    break;
      }

      fprintf(out, " time units = 1e%d\n", ivl_scope_time_units(net));

      for (idx = 0 ;  idx < ivl_scope_attr_cnt(net) ;  idx += 1) {
	    ivl_attribute_t attr = ivl_scope_attr_val(net, idx);
	    switch (attr->type) {
		case IVL_ATT_VOID:
		  fprintf(out, "  (* %s *)\n", attr->key);
		  break;
		case IVL_ATT_STR:
		  fprintf(out, "  (* %s = \"%s\" *)\n", attr->key,
			  attr->val.str);
		  break;
		case IVL_ATT_NUM:
		  fprintf(out, "  (* %s = %ld *)\n", attr->key,
			  attr->val.num);
		  break;
	    }
      }

      for (idx = 0 ;  idx < ivl_scope_params(net) ;  idx += 1)
	    show_parameter(ivl_scope_param(net, idx));

      for (idx = 0 ;  idx < ivl_scope_sigs(net) ;  idx += 1)
	    show_signal(ivl_scope_sig(net, idx));

      for (idx = 0 ;  idx < ivl_scope_events(net) ;  idx += 1)
	    show_event(ivl_scope_event(net, idx));

      for (idx = 0 ;  idx < ivl_scope_logs(net) ;  idx += 1)
	    show_logic(ivl_scope_log(net, idx));

      for (idx = 0 ;  idx < ivl_scope_lpms(net) ;  idx += 1)
	    show_lpm(ivl_scope_lpm(net, idx));

      switch (ivl_scope_type(net)) {
	  case IVL_SCT_FUNCTION:
	  case IVL_SCT_TASK:
	    fprintf(out, "  scope function/task definition\n");
	    show_statement(ivl_scope_def(net), 6);
	    break;

	  default:
	    if (ivl_scope_def(net)) {
		  fprintf(out, "  ERROR: scope has an attached task definition:\n");
		  show_statement(ivl_scope_def(net), 6);
		  stub_errors += 1;
	    }
	    break;
      }

      fprintf(out, "end scope %s\n", ivl_scope_name(net));
      return ivl_scope_children(net, show_scope, 0);
}

static void show_primitive(ivl_udp_t net, unsigned ref_count)
{
      unsigned rdx;

      fprintf(out, "primtive %s (referenced %u times)\n",
	      ivl_udp_name(net), ref_count);

      if (ivl_udp_sequ(net))
	    fprintf(out, "    reg out = %u;\n", ivl_udp_init(net));
      else
	    fprintf(out, "    wire out;\n");
      fprintf(out, "    table\n");
      for (rdx = 0 ;  rdx < ivl_udp_rows(net) ;  rdx += 1) {

	      /* Each row has the format:
		 Combinational: iii...io
		 Sequential:  oiii...io
		 In the sequential case, the o value in the front is
		 the current output value. */
	    unsigned idx;
	    const char*row = ivl_udp_row(net,rdx);

	    fprintf(out, "    ");

	    if (ivl_udp_sequ(net))
		  fprintf(out, " cur=%c :", *row++);

	    for (idx = 0 ;  idx < ivl_udp_nin(net) ;  idx += 1)
		  fprintf(out, " %c", *row++);

	    fprintf(out, " : out=%c\n", *row++);
      }
      fprintf(out, "    endtable\n");

      fprintf(out, "endprimitive\n");
}

int target_design(ivl_design_t des)
{
      ivl_scope_t*root_scopes;
      unsigned nroot = 0;
      unsigned idx;

      const char*path = ivl_design_flag(des, "-o");
      if (path == 0) {
	    return -1;
      }

      out = fopen(path, "w");
      if (out == 0) {
	    perror(path);
	    return -2;
      }

      ivl_design_roots(des, &root_scopes, &nroot);
      for (idx = 0 ;  idx < nroot ;  idx += 1) {

	    fprintf(out, "root module = %s;\n",
		    ivl_scope_name(root_scopes[idx]));
	    show_scope(root_scopes[idx], 0);
      }

      while (udp_define_list) {
	    struct udp_define_cell*cur = udp_define_list;
	    udp_define_list = cur->next;
	    show_primitive(cur->udp, cur->ref);
	    free(cur);
      }

      ivl_design_process(des, show_process, 0);
      fclose(out);

      return stub_errors;
}

