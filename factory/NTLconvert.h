/* $Id: NTLconvert.h,v 1.2 2002-07-30 15:20:02 Singular Exp $ */
#ifndef INCL_NTLCONVERT_H
#define INCL_NTLCONVERT_H

#include <config.h>

#include "cf_gmp.h"

#include "assert.h"

#include "cf_defs.h"
#include "canonicalform.h"
#include "cf_iter.h"
#include "fac_berlekamp.h"
#include "fac_cantzass.h"
#include "fac_univar.h"
#include "fac_multivar.h"
#include "fac_sqrfree.h"
#include "cf_algorithm.h"

#ifdef HAVE_NTL

#include <NTL/ZZXFactoring.h>
#include <NTL/ZZ_pXFactoring.h>
#include <NTL/GF2XFactoring.h>
#include "int_int.h"
#include <limits.h>
#include <NTL/ZZ_pEXFactoring.h>
#include <NTL/GF2EXFactoring.h>

ZZ_pX convertFacCF2NTLZZpX(CanonicalForm f);
GF2X convertFacCF2NTLGF2X(CanonicalForm f);
CanonicalForm convertNTLZZpX2CF(ZZ_pX poly,Variable x);
CanonicalForm convertNTLGF2X2CF(GF2X poly,Variable x);
CFFList convertNTLvec_pair_ZZpX_long2FacCFFList(vec_pair_ZZ_pX_long e,ZZ_p multi,Variable x);

CFFList convertNTLvec_pair_GF2X_long2FacCFFList(vec_pair_GF2X_long e,GF2 multi,Variable x);
CanonicalForm convertZZ2CF(ZZ coefficient);
ZZX convertFacCF2NTLZZX(CanonicalForm f);
CFFList convertNTLvec_pair_ZZX_long2FacCFFList(vec_pair_ZZX_long e,ZZ multi,Variable x);
CanonicalForm convertNTLZZpE2CF(ZZ_pE coefficient,Variable x);
CFFList convertNTLvec_pair_ZZpEX_long2FacCFFList(vec_pair_ZZ_pEX_long e,ZZ_pE multi,Variable x,Variable alpha);
CanonicalForm convertNTLGF2E2CF(GF2E coefficient,Variable x);
CFFList convertNTLvec_pair_GF2EX_long2FacCFFList(vec_pair_GF2EX_long e,GF2E multi,Variable x,Variable alpha);
GF2EX convertFacCF2NTLGF2EX(CanonicalForm f,ZZ_pX mipo);
ZZ_pEX convertFacCF2NTLZZ_pEX(CanonicalForm f,ZZ_pX mipo);

#endif
#endif
