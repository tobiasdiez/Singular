/****************************************
*  Computer Algebra System SINGULAR     *
****************************************/
/* $Id: kstd1.cc,v 1.33 2008-02-24 17:41:31 levandov Exp $ */
/*
* ABSTRACT:
*/

// define if buckets should be used
#define MORA_USE_BUCKETS

// define if tailrings should be used
#define HAVE_TAIL_RING

#include "mod2.h"
#include "structs.h"
#include "omalloc.h"
#include "kutil.h"
#include "kInline.cc"
#include "polys.h"
#include "febase.h"
#include "kstd1.h"
#include "khstd.h"
#include "stairc.h"
#include "weight.h"
//#include "cntrlc.h"
#include "intvec.h"
#include "ideals.h"
//#include "../Singular/ipid.h"
#include "timer.h"

//#include "ipprint.h"

#ifdef HAVE_PLURAL
#include "sca.h"
#endif


/* the list of all options which give a warning by test */
BITSET kOptions=Sy_bit(OPT_PROT)           /*  0 */
                |Sy_bit(OPT_REDSB)         /*  1 */
                |Sy_bit(OPT_NOT_SUGAR)     /*  3 */
                |Sy_bit(OPT_INTERRUPT)     /*  4 */
                |Sy_bit(OPT_SUGARCRIT)     /*  5 */
                |Sy_bit(OPT_REDTHROUGH)
                |Sy_bit(OPT_OLDSTD)
                |Sy_bit(OPT_FASTHC)        /* 10 */
                |Sy_bit(OPT_KEEPVARS)      /* 21 */
                |Sy_bit(OPT_INTSTRATEGY)   /* 26 */
                |Sy_bit(OPT_INFREDTAIL)    /* 28 */
                |Sy_bit(OPT_NOTREGULARITY) /* 30 */
                |Sy_bit(OPT_WEIGHTM);      /* 31 */

/* the list of all options which may be used by option and test */
BITSET validOpts=Sy_bit(0)
                |Sy_bit(1)
                |Sy_bit(2) // obachman 10/00: replaced by notBucket
                |Sy_bit(3)
                |Sy_bit(4)
                |Sy_bit(5)
                |Sy_bit(6)
//                |Sy_bit(7) obachman 11/00 tossed: 12/00 used for redThrough
  |Sy_bit(OPT_REDTHROUGH)
//                |Sy_bit(8) obachman 11/00 tossed
                |Sy_bit(9)
                |Sy_bit(10)
                |Sy_bit(11)
                |Sy_bit(12)
                |Sy_bit(13)
                |Sy_bit(14)
                |Sy_bit(15)
                |Sy_bit(16)
                |Sy_bit(17)
                |Sy_bit(18)
                |Sy_bit(19)
//                |Sy_bit(20) obachman 11/00 tossed: 12/00 used for redOldStd
  |Sy_bit(OPT_OLDSTD)
                |Sy_bit(21)
                |Sy_bit(22)
                /*|Sy_bit(23)*/
                /*|Sy_bit(24)*/
                |Sy_bit(OPT_REDTAIL)
                |Sy_bit(OPT_INTSTRATEGY)
                |Sy_bit(27)
                |Sy_bit(28)
                |Sy_bit(29)
                |Sy_bit(30)
                |Sy_bit(31);

//static BOOLEAN posInLOldFlag;
           /*FALSE, if posInL == posInL10*/
// returns TRUE if mora should use buckets, false otherwise
static BOOLEAN kMoraUseBucket(kStrategy strat);

static void kOptimizeLDeg(pLDegProc ldeg, kStrategy strat)
{
//  if (strat->ak == 0 && !rIsSyzIndexRing(currRing))
    strat->length_pLength = TRUE;
//  else
//    strat->length_pLength = FALSE;

  if ((ldeg == pLDeg0c /*&& !rIsSyzIndexRing(currRing)*/) ||
      (ldeg == pLDeg0 && strat->ak == 0))
  {
    strat->LDegLast = TRUE;
  }
  else
  {
    strat->LDegLast = FALSE;
  }
}


static int doRed (LObject* h, TObject* with,BOOLEAN intoT,kStrategy strat)
{
  poly hp;
  int ret;
#if KDEBUG > 0
  kTest_L(h);
  kTest_T(with);
#endif
  // Hmmm ... why do we do this -- polys from T should already be normalized
  if (!TEST_OPT_INTSTRATEGY)
    with->pNorm();
#ifdef KDEBUG
  if (TEST_OPT_DEBUG)
  {
    PrintS("reduce ");h->wrp();PrintS(" with ");with->wrp();PrintLn();
  }
#endif
  if (intoT)
  {
    // need to do it exacly like this: otherwise
    // we might get errors
    LObject L= *h;
    L.Copy();
    h->GetP();
    h->SetLength(strat->length_pLength);
    ret = ksReducePoly(&L, with, strat->kNoetherTail(), NULL, strat);
    if (ret)
    {
      if (ret < 0) return ret;
      if (h->tailRing != strat->tailRing)
        h->ShallowCopyDelete(strat->tailRing,
                             pGetShallowCopyDeleteProc(h->tailRing,
                                                       strat->tailRing));
    }
    enterT(*h,strat);
    *h = L;
  }
  else
    ret = ksReducePoly(h, with, strat->kNoetherTail(), NULL, strat);
#ifdef KDEBUG
  if (TEST_OPT_DEBUG)
  {
    PrintS("to ");h->wrp();PrintLn();
  }
#endif
  return ret;
}

int redEcart (LObject* h,kStrategy strat)
{
  poly pi;
  int i,at,reddeg,d,ei,li,ii;
  int j = 0;
  int pass = 0;

  d = h->GetpFDeg()+ h->ecart;
  reddeg = strat->LazyDegree+d;
  h->SetShortExpVector();
  loop
  {
    j = kFindDivisibleByInT(strat->T, strat->sevT, strat->tl, h);
    if (j < 0)
    {
      if (strat->honey) h->SetLength(strat->length_pLength);
      return 1;
    }

    ei = strat->T[j].ecart;
    ii = j;

    if (ei > h->ecart && ii < strat->tl)
    {
      li = strat->T[j].length;
      // the polynomial to reduce with (up to the moment) is;
      // pi with ecart ei and length li
      // look for one with smaller ecart
      i = j;
      loop
      {
        /*- takes the first possible with respect to ecart -*/
        i++;
#if 1
        if (i > strat->tl) break;
        if ((strat->T[i].ecart < ei || (strat->T[i].ecart == ei &&
                                        strat->T[i].length < li))
            &&
            p_LmShortDivisibleBy(strat->T[i].GetLmTailRing(), strat->sevT[i], h->GetLmTailRing(), ~h->sev, strat->tailRing))
#else
          j = kFindDivisibleByInT(strat->T, strat->sevT, strat->tl, h, i);
        if (j < 0) break;
        i = j;
        if (strat->T[i].ecart < ei || (strat->T[i].ecart == ei &&
                                        strat->T[i].length < li))
#endif
        {
          // the polynomial to reduce with is now
          ii = i;
          ei = strat->T[i].ecart;
          if (ei <= h->ecart) break;
          li = strat->T[i].length;
        }
      }
    }

    // end of search: have to reduce with pi
    if (ei > h->ecart)
    {
      // It is not possible to reduce h with smaller ecart;
      // if possible h goes to the lazy-set L,i.e
      // if its position in L would be not the last one
      strat->fromT = TRUE;
      if (!K_TEST_OPT_REDTHROUGH && strat->Ll >= 0) /*- L is not empty -*/
      {
        h->SetLmCurrRing();
        if (strat->honey && strat->posInLDependsOnLength)
          h->SetLength(strat->length_pLength);
        assume(h->FDeg == h->pFDeg());
        at = strat->posInL(strat->L,strat->Ll,h,strat);
        if (at <= strat->Ll)
        {
          /*- h will not become the next element to reduce -*/
          enterL(&strat->L,&strat->Ll,&strat->Lmax,*h,at);
#ifdef KDEBUG
          if (TEST_OPT_DEBUG) Print(" ecart too big; -> L%d\n",at);
#endif
          h->Clear();
          strat->fromT = FALSE;
          return -1;
        }
      }
    }

    // now we finally can reduce
    doRed(h,&(strat->T[ii]),strat->fromT,strat);
    strat->fromT=FALSE;

    // are we done ???
    if (h->IsNull())
    {
      if (h->lcm!=NULL) pLmFree(h->lcm);
      h->Clear();
      return 0;
    }

    // NO!
    h->SetShortExpVector();
    h->SetpFDeg();
    if (strat->honey)
    {
      if (ei <= h->ecart)
        h->ecart = d-h->GetpFDeg();
      else
        h->ecart = d-h->GetpFDeg()+ei-h->ecart;
    }
    else
      // this has the side effect of setting h->length
      h->ecart = h->pLDeg(strat->LDegLast) - h->GetpFDeg();
#if 0
    if (strat->syzComp!=0)
    {
      if ((strat->syzComp>0) && (h->Comp() > strat->syzComp))
      {
        assume(h->MinComp() > strat->syzComp);
        if (strat->honey) h->SetLength();
#ifdef KDEBUG
        if (TEST_OPT_DEBUG) PrintS(" > syzComp\n");
#endif
        return -2;
      }
    }
#endif
    /*- try to reduce the s-polynomial -*/
    pass++;
    d = h->GetpFDeg()+h->ecart;
    /*
     *test whether the polynomial should go to the lazyset L
     *-if the degree jumps
     *-if the number of pre-defined reductions jumps
     */
    if (!K_TEST_OPT_REDTHROUGH && (strat->Ll >= 0)
        && ((d >= reddeg) || (pass > strat->LazyPass)))
    {
      h->SetLmCurrRing();
      if (strat->honey && strat->posInLDependsOnLength)
        h->SetLength(strat->length_pLength);
      assume(h->FDeg == h->pFDeg());
      at = strat->posInL(strat->L,strat->Ll,h,strat);
      if (at <= strat->Ll)
      {
        int dummy=strat->sl;
        if (kFindDivisibleByInS(strat, &dummy, h) < 0)
        {
          if (strat->honey && !strat->posInLDependsOnLength)
            h->SetLength(strat->length_pLength);
          return 1;
        }
        enterL(&strat->L,&strat->Ll,&strat->Lmax,*h,at);
#ifdef KDEBUG
        if (TEST_OPT_DEBUG) Print(" degree jumped; ->L%d\n",at);
#endif
        h->Clear();
        return -1;
      }
    }
    else if ((TEST_OPT_PROT) && (strat->Ll < 0) && (d >= reddeg))
    {
      Print(".%d",d);mflush();
      reddeg = d+1;
    }
  }
}

/*2
*reduces h with elements from T choosing  the first possible
* element in t with respect to the given pDivisibleBy
*/
int redFirst (LObject* h,kStrategy strat)
{
  if (h->IsNull()) return 0;

  int at, reddeg,d;
  int pass = 0;
  int j = 0;

  if (! strat->homog)
  {
    d = h->GetpFDeg() + h->ecart;
    reddeg = strat->LazyDegree+d;
  }
  h->SetShortExpVector();
  loop
  {
    j = kFindDivisibleByInT(strat->T, strat->sevT, strat->tl, h);
    if (j < 0)
    {
      h->SetDegStuffReturnLDeg(strat->LDegLast);
      return 1;
    }

    if (!TEST_OPT_INTSTRATEGY)
      strat->T[j].pNorm();
#ifdef KDEBUG
    if (TEST_OPT_DEBUG)
    {
      PrintS("reduce ");
      h->wrp();
      PrintS(" with ");
      strat->T[j].wrp();
    }
#endif
    ksReducePoly(h, &(strat->T[j]), strat->kNoetherTail(), NULL, strat);
#ifdef KDEBUG
    if (TEST_OPT_DEBUG)
    {
      PrintS(" to ");
      wrp(h->p);
      PrintLn();
    }
#endif
    if (h->IsNull())
    {
      if (h->lcm!=NULL) pLmFree(h->lcm);
      h->Clear();
      return 0;
    }
    h->SetShortExpVector();

#if 0
    if ((strat->syzComp!=0) && !strat->honey)
    {
      if ((strat->syzComp>0) &&
          (h->Comp() > strat->syzComp))
      {
        assume(h->MinComp() > strat->syzComp);
#ifdef KDEBUG
        if (TEST_OPT_DEBUG) PrintS(" > syzComp\n");
#endif
        if (strat->homog)
          h->SetDegStuffReturnLDeg(strat->LDegLast);
        return -2;
      }
    }
#endif
    if (!strat->homog)
    {
      if (!K_TEST_OPT_OLDSTD && strat->honey)
      {
        h->SetpFDeg();
        if (strat->T[j].ecart <= h->ecart)
          h->ecart = d - h->GetpFDeg();
        else
          h->ecart = d - h->GetpFDeg() + strat->T[j].ecart - h->ecart;

        d = h->GetpFDeg() + h->ecart;
      }
      else
        d = h->SetDegStuffReturnLDeg(strat->LDegLast);
      /*- try to reduce the s-polynomial -*/
      pass++;
      /*
       *test whether the polynomial should go to the lazyset L
       *-if the degree jumps
       *-if the number of pre-defined reductions jumps
       */
      if (!K_TEST_OPT_REDTHROUGH && (strat->Ll >= 0)
          && ((d >= reddeg) || (pass > strat->LazyPass)))
      {
        h->SetLmCurrRing();
        if (strat->posInLDependsOnLength)
          h->SetLength(strat->length_pLength);
        at = strat->posInL(strat->L,strat->Ll,h,strat);
        if (at <= strat->Ll)
        {
          int dummy=strat->sl;
          if (kFindDivisibleByInS(strat,&dummy, h) < 0)
            return 1;
          enterL(&strat->L,&strat->Ll,&strat->Lmax,*h,at);
#ifdef KDEBUG
          if (TEST_OPT_DEBUG) Print(" degree jumped; ->L%d\n",at);
#endif
          h->Clear();
          return -1;
        }
      }
      if ((TEST_OPT_PROT) && (strat->Ll < 0) && (d >= reddeg))
      {
        reddeg = d+1;
        Print(".%d",d);mflush();
      }
    }
  }
}

/*2
* reduces h with elements from T choosing first possible
* element in T with respect to the given ecart
* used for computing normal forms outside kStd
*/
static poly redMoraNF (poly h,kStrategy strat, int flag)
{
  LObject H;
  H.p = h;
  int j = 0;
  int z = 10;
  int o = H.SetpFDeg();
  H.ecart = pLDeg(H.p,&H.length,currRing)-o;
  if ((flag & 2) == 0) cancelunit(&H,TRUE);
  H.sev = pGetShortExpVector(H.p);
  unsigned long not_sev = ~ H.sev;
  loop
  {
    if (j > strat->tl)
    {
      return H.p;
    }
    if (TEST_V_DEG_STOP)
    {
      if (kModDeg(H.p)>Kstd1_deg) pDeleteLm(&H.p);
      if (H.p==NULL) return NULL;
    }
    if (p_LmShortDivisibleBy(strat->T[j].GetLmTailRing(), strat->sevT[j], H.GetLmTailRing(), not_sev, strat->tailRing))
    {
      //if (strat->interpt) test_int_std(strat->kIdeal);
      /*- remember the found T-poly -*/
      poly pi = strat->T[j].p;
      int ei = strat->T[j].ecart;
      int li = strat->T[j].length;
      int ii = j;
      /*
      * the polynomial to reduce with (up to the moment) is;
      * pi with ecart ei and length li
      */
      loop
      {
        /*- look for a better one with respect to ecart -*/
        /*- stop, if the ecart is small enough (<=ecart(H)) -*/
        j++;
        if (j > strat->tl) break;
        if (ei <= H.ecart) break;
        if (((strat->T[j].ecart < ei)
          || ((strat->T[j].ecart == ei)
        && (strat->T[j].length < li)))
        && pLmShortDivisibleBy(strat->T[j].p,strat->sevT[j], H.p, not_sev))
        {
          /*
          * the polynomial to reduce with is now;
          */
          pi = strat->T[j].p;
          ei = strat->T[j].ecart;
          li = strat->T[j].length;
          ii = j;
        }
      }
      /*
      * end of search: have to reduce with pi
      */
      z++;
      if (z>10)
      {
        pNormalize(H.p);
        z=0;
      }
      if ((ei > H.ecart) && (!strat->kHEdgeFound))
      {
        /*
        * It is not possible to reduce h with smaller ecart;
        * we have to reduce with bad ecart: H has to enter in T
        */
        doRed(&H,&(strat->T[ii]),TRUE,strat);
        if (H.p == NULL)
          return NULL;
      }
      else
      {
        /*
        * we reduce with good ecart, h need not to be put to T
        */
        doRed(&H,&(strat->T[ii]),FALSE,strat);
        if (H.p == NULL)
          return NULL;
      }
      /*- try to reduce the s-polynomial -*/
      o = H.SetpFDeg();
      if ((flag &2 ) == 0) cancelunit(&H,TRUE);
      H.ecart = pLDeg(H.p,&(H.length),currRing)-o;
      j = 0;
      H.sev = pGetShortExpVector(H.p);
      not_sev = ~ H.sev;
    }
    else
    {
      j++;
    }
  }
}

/*2
*reorders  L with respect to posInL
*/
void reorderL(kStrategy strat)
{
  int i,j,at;
  LObject p;

  for (i=1; i<=strat->Ll; i++)
  {
    at = strat->posInL(strat->L,i-1,&(strat->L[i]),strat);
    if (at != i)
    {
      p = strat->L[i];
      for (j=i-1; j>=at; j--) strat->L[j+1] = strat->L[j];
      strat->L[at] = p;
    }
  }
}

/*2
*reorders  T with respect to length
*/
void reorderT(kStrategy strat)
{
  int i,j,at;
  TObject p;
  unsigned long sev;


  for (i=1; i<=strat->tl; i++)
  {
    if (strat->T[i-1].length > strat->T[i].length)
    {
      p = strat->T[i];
      sev = strat->sevT[i];
      at = i-1;
      loop
      {
        at--;
        if (at < 0) break;
        if (strat->T[i].length > strat->T[at].length) break;
      }
      for (j = i-1; j>at; j--)
      {
        strat->T[j+1]=strat->T[j];
        strat->sevT[j+1]=strat->sevT[j];
        strat->R[strat->T[j+1].i_r] = &(strat->T[j+1]);
      }
      strat->T[at+1]=p;
      strat->sevT[at+1] = sev;
      strat->R[p.i_r] = &(strat->T[at+1]);
    }
  }
}

/*2
*looks whether exactly pVariables-1 axis are used
*returns last != 0 in this case
*last is the (first) unused axis
*/
void missingAxis (int* last,kStrategy strat)
{
  int   i = 0;
  int   k = 0;

  *last = 0;
  if (!currRing->MixedOrder)
  {
    loop
    {
      i++;
      if (i > pVariables) break;
      if (strat->NotUsedAxis[i])
      {
        *last = i;
        k++;
      }
      if (k>1)
      {
        *last = 0;
        break;
      }
    }
  }
}

/*2
*last is the only non used axis, it looks
*for a monomial in p being a pure power of this
*variable and returns TRUE in this case
*(*length) gives the length between the pure power and the leading term
*(should be minimal)
*/
BOOLEAN hasPurePower (const poly p,int last, int *length,kStrategy strat)
{
  poly h;
  int i;

  if (pNext(p) == strat->tail)
    return FALSE;
  pp_Test(p, currRing, strat->tailRing);
  if (strat->ak <= 0 || p_MinComp(p, currRing, strat->tailRing) == strat->ak)
  {
    i = p_IsPurePower(p, currRing);
    if (i == last)
    {
      *length = 0;
      return TRUE;
    }
    *length = 1;
    h = pNext(p);
    while (h != NULL)
    {
      i = p_IsPurePower(h, strat->tailRing);
      if (i==last) return TRUE;
      (*length)++;
      pIter(h);
    }
  }
  return FALSE;
}

BOOLEAN hasPurePower (LObject *L,int last, int *length,kStrategy strat)
{
  if (L->bucket != NULL)
  {
    poly p = L->CanonicalizeP();
    BOOLEAN ret = hasPurePower(p, last, length, strat);
    pNext(p) = NULL;
    return ret;
  }
  else
  {
    return hasPurePower(L->p, last, length, strat);
  }
}

/*2
* looks up the position of polynomial p in L
* in the case of looking for the pure powers
*/
int posInL10 (const LSet set,const int length, LObject* p,const kStrategy strat)
{
  int j,dp,dL;

  if (length<0) return 0;
  if (hasPurePower(p,strat->lastAxis,&dp,strat))
  {
    int op= p->GetpFDeg() +p->ecart;
    for (j=length; j>=0; j--)
    {
      if (!hasPurePower(&(set[j]),strat->lastAxis,&dL,strat))
        return j+1;
      if (dp < dL)
        return j+1;
      if ((dp == dL)
          && (set[j].GetpFDeg()+set[j].ecart >= op))
        return j+1;
    }
  }
  j=length;
  loop
  {
    if (j<0) break;
    if (!hasPurePower(&(set[j]),strat->lastAxis,&dL,strat)) break;
    j--;
  }
  return strat->posInLOld(set,j,p,strat);
}


/*2
* computes the s-polynomials L[ ].p in L
*/
void updateL(kStrategy strat)
{
  LObject p;
  int dL;
  int j=strat->Ll;
  loop
  {
    if (j<0) break;
    if (hasPurePower(&(strat->L[j]),strat->lastAxis,&dL,strat))
    {
      p=strat->L[strat->Ll];
      strat->L[strat->Ll]=strat->L[j];
      strat->L[j]=p;
      break;
    }
    j--;
  }
  if (j<0)
  {
    j=strat->Ll;
    loop
    {
      if (j<0) break;
      if (pNext(strat->L[j].p) == strat->tail)
      {
#ifdef HAVE_RINGS
        if (rField_is_Ring(currRing))
          pLmDelete(strat->L[j].p);    /*deletes the short spoly and computes*/
        else
#else
          pLmFree(strat->L[j].p);    /*deletes the short spoly and computes*/
#endif
        strat->L[j].p = NULL;
        poly m1 = NULL, m2 = NULL;
        // check that spoly creation is ok
        while (strat->tailRing != currRing &&
               !kCheckSpolyCreation(&(strat->L[j]), strat, m1, m2))
        {
          assume(m1 == NULL && m2 == NULL);
          // if not, change to a ring where exponents are at least
          // large enough
          kStratChangeTailRing(strat);
        }
        /* create the real one */
        ksCreateSpoly(&(strat->L[j]), strat->kNoetherTail(), FALSE,
                      strat->tailRing, m1, m2, strat->R);

        strat->L[j].SetLmCurrRing();
        if (!strat->honey)
          strat->initEcart(&strat->L[j]);
        else
          strat->L[j].SetLength(strat->length_pLength);

        BOOLEAN pp = hasPurePower(&(strat->L[j]),strat->lastAxis,&dL,strat);

        if (strat->use_buckets) strat->L[j].PrepareRed(TRUE);

        if (pp)
        {
          p=strat->L[strat->Ll];
          strat->L[strat->Ll]=strat->L[j];
          strat->L[j]=p;
          break;
        }
      }
      j--;
    }
  }
}

/*2
* computes the s-polynomials L[ ].p in L and
* cuts elements in L above noether
*/
void updateLHC(kStrategy strat)
{
  int i = 0;
  kTest_TS(strat);
  while (i <= strat->Ll)
  {
    if (pNext(strat->L[i].p) == strat->tail)
    {
       /*- deletes the int spoly and computes -*/
      if (pLmCmp(strat->L[i].p,strat->kNoether) == -1)
      {
        pLmFree(strat->L[i].p);
        strat->L[i].p = NULL;
      }
      else
      {
        pLmFree(strat->L[i].p);
        strat->L[i].p = NULL;
        poly m1 = NULL, m2 = NULL;
        // check that spoly creation is ok
        while (strat->tailRing != currRing &&
               !kCheckSpolyCreation(&(strat->L[i]), strat, m1, m2))
        {
          assume(m1 == NULL && m2 == NULL);
          // if not, change to a ring where exponents are at least
          // large enough
          kStratChangeTailRing(strat);
        }
        /* create the real one */
        ksCreateSpoly(&(strat->L[i]), strat->kNoetherTail(), FALSE,
                      strat->tailRing, m1, m2, strat->R);
        if (! strat->L[i].IsNull())
        {
          strat->L[i].SetLmCurrRing();
          strat->L[i].SetpFDeg();
          strat->L[i].ecart
            = strat->L[i].pLDeg(strat->LDegLast) - strat->L[i].GetpFDeg();
          if (strat->use_buckets) strat->L[i].PrepareRed(TRUE);
        }
      }
    }
    else
      deleteHC(&(strat->L[i]), strat);
   if (strat->L[i].IsNull())
      deleteInL(strat->L,&strat->Ll,i,strat);
    else
    {
#ifdef KDEBUG
      kTest_L(&(strat->L[i]), strat->tailRing, TRUE, i, strat->T, strat->tl);
#endif
      i++;
    }
  }
  kTest_TS(strat);
}

/*2
* cuts in T above strat->kNoether and tries to cancel a unit
*/
void updateT(kStrategy strat)
{
  int i = 0;
  LObject p;

  while (i <= strat->tl)
  {
    p = strat->T[i];
    deleteHC(&p,strat, TRUE);
    /*- tries to cancel a unit: -*/
    cancelunit(&p);
    if (p.p != strat->T[i].p)
    {
      strat->sevT[i] = pGetShortExpVector(p.p);
      p.SetpFDeg();
    }
    strat->T[i] = p;
    i++;
  }
}

/*2
* arranges red, pos and T if strat->kHEdgeFound (first time)
*/
void firstUpdate(kStrategy strat)
{
  if (strat->update)
  {
    kTest_TS(strat);
    strat->update = (strat->tl == -1);
    if (TEST_OPT_WEIGHTM)
    {
      pRestoreDegProcs(pFDegOld, pLDegOld);
      if (strat->tailRing != currRing)
      {
        strat->tailRing->pFDeg = strat->pOrigFDeg_TailRing;
        strat->tailRing->pLDeg = strat->pOrigLDeg_TailRing;
      }
      int i;
      for (i=strat->Ll; i>=0; i--)
      {
        strat->L[i].SetpFDeg();
      }
      for (i=strat->tl; i>=0; i--)
      {
        strat->T[i].SetpFDeg();
      }
      if (ecartWeights)
      {
        omFreeSize((ADDRESS)ecartWeights,(pVariables+1)*sizeof(short));
        ecartWeights=NULL;
      }
    }
    if (TEST_OPT_FASTHC)
    {
      strat->posInL = strat->posInLOld;
      strat->lastAxis = 0;
    }
    if (BTEST1(27))
      return;
    strat->red = redFirst;
    strat->use_buckets = kMoraUseBucket(strat);
    updateT(strat);
    strat->posInT = posInT2;
    reorderT(strat);
  }
  kTest_TS(strat);
}

/*2
*-puts p to the standardbasis s at position at
*-reduces the tail of p if TEST_OPT_REDTAIL
*-tries to cancel a unit
*-HEckeTest
*  if TRUE
*  - decides about reduction-strategies
*  - computes noether
*  - stops computation if BTEST1(27)
*  - cuts the tails of the polynomials
*    in s,t and the elements in L above noether
*    and cancels units if possible
*  - reorders s,L
*/
void enterSMora (LObject p,int atS,kStrategy strat, int atR = -1)
{
  int i;
  enterSBba(p, atS, strat, atR);
  #ifdef KDEBUG
  if (TEST_OPT_DEBUG)
  {
    Print("new s%d:",atS);
    wrp(p.p);
    PrintLn();
  }
  #endif
  if ((!strat->kHEdgeFound) || (strat->kNoether!=NULL)) HEckeTest(p.p,strat);
  if (strat->kHEdgeFound)
  {
    if (newHEdge(strat->S,strat))
    {
      firstUpdate(strat);
      if (BTEST1(27))
        return;
      /*- cuts elements in L above noether and reorders L -*/
      updateLHC(strat);
      /*- reorders L with respect to posInL -*/
      reorderL(strat);
    }
  }
  else if (strat->kNoether!=NULL)
    strat->kHEdgeFound = TRUE;
  else if (TEST_OPT_FASTHC)
  {
    if (strat->posInLOldFlag)
    {
      missingAxis(&strat->lastAxis,strat);
      if (strat->lastAxis)
      {
        strat->posInLOld = strat->posInL;
        strat->posInLOldFlag = FALSE;
        strat->posInL = posInL10;
        strat->posInLDependsOnLength = TRUE;
        updateL(strat);
        reorderL(strat);
      }
    }
    else if (strat->lastAxis)
      updateL(strat);
  }
}

/*2
*-puts p to the standardbasis s at position at
*-HEckeTest
*  if TRUE
*  - computes noether
*/
void enterSMoraNF (LObject p, int atS,kStrategy strat, int atR = -1)
{
  int i;

  enterSBba(p, atS, strat, atR);
  if ((!strat->kHEdgeFound) || (strat->kNoether!=NULL)) HEckeTest(p.p,strat);
  if (strat->kHEdgeFound)
    newHEdge(strat->S,strat);
  else if (strat->kNoether!=NULL)
    strat->kHEdgeFound = TRUE;
}

void initBba(ideal F,kStrategy strat)
{
  int i;
  idhdl h;
 /* setting global variables ------------------- */
  strat->enterS = enterSBba;
    strat->red = redHoney;
  if (strat->honey)
    strat->red = redHoney;
  else if (pLexOrder && !strat->homog)
    strat->red = redLazy;
  else
  {
    strat->LazyPass *=4;
    strat->red = redHomog;
  }
#ifdef HAVE_RINGS  //TODO Oliver
  if (rField_is_Ring(currRing)) {
    strat->red = redRing;
  }
#endif
  if (pLexOrder && strat->honey)
    strat->initEcart = initEcartNormal;
  else
    strat->initEcart = initEcartBBA;
  if (strat->honey)
    strat->initEcartPair = initEcartPairMora;
  else
    strat->initEcartPair = initEcartPairBba;
  strat->kIdeal = NULL;
  //if (strat->ak==0) strat->kIdeal->rtyp=IDEAL_CMD;
  //else              strat->kIdeal->rtyp=MODUL_CMD;
  //strat->kIdeal->data=(void *)strat->Shdl;
  if ((TEST_OPT_WEIGHTM)&&(F!=NULL))
  {
    //interred  machen   Aenderung
    pFDegOld=pFDeg;
    pLDegOld=pLDeg;
    //h=ggetid("ecart");
    //if ((h!=NULL) /*&& (IDTYP(h)==INTVEC_CMD)*/)
    //{
    //  ecartWeights=iv2array(IDINTVEC(h));
    //}
    //else
    {
      ecartWeights=(short *)omAlloc((pVariables+1)*sizeof(short));
      /*uses automatic computation of the ecartWeights to set them*/
      kEcartWeights(F->m,IDELEMS(F)-1,ecartWeights);
    }
    pRestoreDegProcs(totaldegreeWecart, maxdegreeWecart);
    if (TEST_OPT_PROT)
    {
      for(i=1; i<=pVariables; i++)
        Print(" %d",ecartWeights[i]);
      PrintLn();
      mflush();
    }
  }
}

void initMora(ideal F,kStrategy strat)
{
  int i,j;
  idhdl h;

  strat->NotUsedAxis = (BOOLEAN *)omAlloc((pVariables+1)*sizeof(BOOLEAN));
  for (j=pVariables; j>0; j--) strat->NotUsedAxis[j] = TRUE;
  strat->enterS = enterSMora;
  strat->initEcartPair = initEcartPairMora; /*- ecart approximation -*/
  strat->posInLOld = strat->posInL;
  strat->posInLOldFlag = TRUE;
  strat->initEcart = initEcartNormal;
  strat->kHEdgeFound = ppNoether != NULL;
  if ( strat->kHEdgeFound )
     strat->kNoether = pCopy(ppNoether);
  else if (strat->kHEdgeFound || strat->homog)
    strat->red = redFirst;  /*take the first possible in T*/
#ifdef HAVE_RINGS  //TODO Oliver
  else if (rField_is_Ring(currRing))
    strat->red = redRing;
#endif
  else
    strat->red = redEcart;/*take the first possible in under ecart-restriction*/
  if (strat->kHEdgeFound)
  {
    strat->HCord = pFDeg(ppNoether,currRing)+1;
    strat->posInT = posInT2;
  }
  else
  {
    strat->HCord = 32000;/*- very large -*/
  }
  /*reads the ecartWeights used for Graebes method from the
   *intvec ecart and set ecartWeights
   */
  if ((TEST_OPT_WEIGHTM)&&(F!=NULL))
  {
    //interred  machen   Aenderung
    pFDegOld=pFDeg;
    pLDegOld=pLDeg;
    //h=ggetid("ecart");
    //if ((h!=NULL) /*&& (IDTYP(h)==INTVEC_CMD)*/)
    //{
    //  ecartWeights=iv2array(IDINTVEC(h));
    //}
    //else
    {
      ecartWeights=(short *)omAlloc((pVariables+1)*sizeof(short));
      /*uses automatic computation of the ecartWeights to set them*/
      kEcartWeights(F->m,IDELEMS(F)-1,ecartWeights);
    }

    pSetDegProcs(totaldegreeWecart, maxdegreeWecart);
    if (TEST_OPT_PROT)
    {
      for(i=1; i<=pVariables; i++)
        Print(" %d",ecartWeights[i]);
      PrintLn();
      mflush();
    }
  }
  kOptimizeLDeg(pLDeg, strat);
}

#ifdef HAVE_ASSUME
static int mora_count = 0;
static int mora_loop_count;
#endif

ideal mora (ideal F, ideal Q,intvec *w,intvec *hilb,kStrategy strat)
{
#ifdef HAVE_ASSUME
  mora_count++;
  mora_loop_count = 0;
#endif
#ifdef KDEBUG
  om_Opts.MinTrack = 5;
#endif
  int srmax;
  int lrmax = 0;
  int olddeg = 0;
  int reduc = 0;
  int red_result = 1;
  int hilbeledeg=1,hilbcount=0;

  strat->update = TRUE;
  /*- setting global variables ------------------- -*/
  initBuchMoraCrit(strat);
  initHilbCrit(F,Q,&hilb,strat);
  initMora(F,strat);
  initBuchMoraPos(strat);
  /*Shdl=*/initBuchMora(F,Q,strat);
  if (TEST_OPT_FASTHC) missingAxis(&strat->lastAxis,strat);
  /*updateS in initBuchMora has Hecketest
  * and could have put strat->kHEdgdeFound FALSE*/
  if (ppNoether!=NULL)
  {
    strat->kHEdgeFound = TRUE;
  }
  if (strat->kHEdgeFound && strat->update)
  {
    firstUpdate(strat);
    updateLHC(strat);
    reorderL(strat);
  }
  if (TEST_OPT_FASTHC && (strat->lastAxis) && strat->posInLOldFlag)
  {
    strat->posInLOld = strat->posInL;
    strat->posInLOldFlag = FALSE;
    strat->posInL = posInL10;
    updateL(strat);
    reorderL(strat);
  }
  srmax = strat->sl;
  kTest_TS(strat);
  strat->use_buckets = kMoraUseBucket(strat);
  /*- compute-------------------------------------------*/

#ifdef HAVE_TAIL_RING
//  if (strat->homog && strat->red == redFirst)
    kStratInitChangeTailRing(strat);
#endif

  while (strat->Ll >= 0)
  {
#ifdef HAVE_ASSUME
    mora_loop_count++;
#endif
    if (lrmax< strat->Ll) lrmax=strat->Ll; /*stat*/
    //test_int_std(strat->kIdeal);
    #ifdef KDEBUG
    if (TEST_OPT_DEBUG) messageSets(strat);
    #endif
    if (TEST_OPT_DEGBOUND
    && (strat->L[strat->Ll].ecart+strat->L[strat->Ll].GetpFDeg()> Kstd1_deg))
    {
      /*
      * stops computation if
      * - 24 (degBound)
      *   && upper degree is bigger than Kstd1_deg
      */
      while ((strat->Ll >= 0)
        && (strat->L[strat->Ll].p1!=NULL) && (strat->L[strat->Ll].p2!=NULL)
        && (strat->L[strat->Ll].ecart+strat->L[strat->Ll].GetpFDeg()> Kstd1_deg)
      )
      {
        deleteInL(strat->L,&strat->Ll,strat->Ll,strat);
        //if (TEST_OPT_PROT)
        //{
        //   PrintS("D"); mflush();
        //}
      }
      if (strat->Ll<0) break;
      else strat->noClearS=TRUE;
    }
    strat->P = strat->L[strat->Ll];/*- picks the last element from the lazyset L -*/
    if (strat->Ll==0) strat->interpt=TRUE;
    strat->Ll--;

    // create the real Spoly
    if (pNext(strat->P.p) == strat->tail)
    {
      /*- deletes the short spoly and computes -*/
      pLmFree(strat->P.p);
      strat->P.p = NULL;
      poly m1 = NULL, m2 = NULL;
      // check that spoly creation is ok
      while (strat->tailRing != currRing &&
             !kCheckSpolyCreation(&(strat->P), strat, m1, m2))
      {
        assume(m1 == NULL && m2 == NULL);
        // if not, change to a ring where exponents are large enough
        kStratChangeTailRing(strat);
      }
      /* create the real one */
      ksCreateSpoly(&(strat->P), strat->kNoetherTail(), strat->use_buckets,
                    strat->tailRing, m1, m2, strat->R);
      if (!strat->use_buckets)
        strat->P.SetLength(strat->length_pLength);
    }
    else if (strat->P.p1 == NULL)
    {
      // for input polys, prepare reduction (buckets !)
      strat->P.SetLength(strat->length_pLength);
      strat->P.PrepareRed(strat->use_buckets);
    }

    if (!strat->P.IsNull())
    {
      // might be NULL from noether !!!
      if (TEST_OPT_PROT)
        message(strat->P.ecart+strat->P.GetpFDeg(),&olddeg,&reduc,strat, red_result);
      // reduce
      red_result = strat->red(&strat->P,strat);
    }

    if (! strat->P.IsNull())
    {
      strat->P.GetP();
      // statistics
      if (TEST_OPT_PROT) PrintS("s");
      // normalization
      if (!TEST_OPT_INTSTRATEGY)
        strat->P.pNorm();
      // tailreduction
      strat->P.p = redtail(&(strat->P),strat->sl,strat);
      // set ecart -- might have changed because of tail reductions
      if ((!strat->noTailReduction) && (!strat->honey))
        strat->initEcart(&strat->P);
      // cancel unit
      cancelunit(&strat->P);
      // for char 0, clear denominators
      if (TEST_OPT_INTSTRATEGY)
        strat->P.pCleardenom();

      // put in T
      enterT(strat->P,strat);
      // build new pairs
      enterpairs(strat->P.p,strat->sl,strat->P.ecart,0,strat, strat->tl);
      // put in S
      strat->enterS(strat->P,
                    posInS(strat,strat->sl,strat->P.p, strat->P.ecart),
                    strat, strat->tl);

      // apply hilbert criterion
      if (hilb!=NULL) khCheck(Q,w,hilb,hilbeledeg,hilbcount,strat);

      // clear strat->P
      if (strat->P.lcm!=NULL) pLmFree(strat->P.lcm);
      strat->P.lcm=NULL;
#ifdef KDEBUG
      // make sure kTest_TS does not complain about strat->P
      memset(&strat->P,0,sizeof(strat->P));
#endif
      if (strat->sl>srmax) srmax = strat->sl; /*stat.*/
      if (strat->Ll>lrmax) lrmax = strat->Ll;
    }
    if (strat->kHEdgeFound)
    {
      if ((BTEST1(27))
      || ((TEST_OPT_MULTBOUND) && (scMult0Int((strat->Shdl)) < mu)))
      {
        // obachman: is this still used ???
        /*
        * stops computation if strat->kHEdgeFound and
        * - 27 (finiteDeterminacyTest)
        * or
        * - 23
        *   (multBound)
        *   && multiplicity of the ideal is smaller then a predefined number mu
        */
        while (strat->Ll >= 0) deleteInL(strat->L,&strat->Ll,strat->Ll,strat);
      }
    }
    kTest_TS(strat);
  }
  /*- complete reduction of the standard basis------------------------ -*/
  if (TEST_OPT_REDSB) completeReduce(strat);
  else if (TEST_OPT_PROT) PrintLn();
  /*- release temp data------------------------------- -*/
  exitBuchMora(strat);
  /*- polynomials used for HECKE: HC, noether -*/
  if (BTEST1(27))
  {
    if (strat->kHEdge!=NULL)
      Kstd1_mu=pFDeg(strat->kHEdge,currRing);
    else
      Kstd1_mu=-1;
  }
  pDelete(&strat->kHEdge);
  strat->update = TRUE; //???
  strat->lastAxis = 0; //???
  pDelete(&strat->kNoether);
  omFreeSize((ADDRESS)strat->NotUsedAxis,(pVariables+1)*sizeof(BOOLEAN));
  if (TEST_OPT_PROT) messageStat(srmax,lrmax,hilbcount,strat);
  if (TEST_OPT_WEIGHTM)
  {
    pRestoreDegProcs(pFDegOld, pLDegOld);
    if (ecartWeights)
    {
      omFreeSize((ADDRESS)ecartWeights,(pVariables+1)*sizeof(short));
      ecartWeights=NULL;
    }
  }
  if (Q!=NULL) updateResult(strat->Shdl,Q,strat);
  idTest(strat->Shdl);
  return (strat->Shdl);
}

poly kNF1 (ideal F,ideal Q,poly q, kStrategy strat, int lazyReduce)
{
  poly   p;
  int   i;
  int   j;
  int   o;
  LObject   h;
  BITSET save_test=test;

  if ((idIs0(F))&&(Q==NULL))
    return pCopy(q); /*F=0*/
  strat->ak = si_max(idRankFreeModule(F),pMaxComp(q));
  /*- creating temp data structures------------------- -*/
  strat->kHEdgeFound = ppNoether != NULL;
  strat->kNoether    = pCopy(ppNoether);
  test|=Sy_bit(OPT_REDTAIL);
  test&=~Sy_bit(OPT_INTSTRATEGY);
  if (TEST_OPT_STAIRCASEBOUND
  && (! TEST_V_DEG_STOP)
  && (0<Kstd1_deg)
  && ((!strat->kHEdgeFound)
    ||(TEST_OPT_DEGBOUND && (pWTotaldegree(strat->kNoether)<Kstd1_deg))))
  {
    pDelete(&strat->kNoether);
    strat->kNoether=pOne();
    pSetExp(strat->kNoether,1, Kstd1_deg+1);
    pSetm(strat->kNoether);
    strat->kHEdgeFound=TRUE;
  }
  initBuchMoraCrit(strat);
  initBuchMoraPos(strat);
  initMora(F,strat);
  strat->enterS = enterSMoraNF;
  /*- set T -*/
  strat->tl = -1;
  strat->tmax = setmaxT;
  strat->T = initT();
  strat->R = initR();
  strat->sevT = initsevT();
  /*- set S -*/
  strat->sl = -1;
  /*- init local data struct.-------------------------- -*/
  /*Shdl=*/initS(F,Q,strat);
  if ((strat->ak!=0)
  && (strat->kHEdgeFound))
  {
    if (strat->ak!=1)
    {
      pSetComp(strat->kNoether,1);
      pSetmComp(strat->kNoether);
      poly p=pHead(strat->kNoether);
      pSetComp(p,strat->ak);
      pSetmComp(p);
      p=pAdd(strat->kNoether,p);
      strat->kNoether=pNext(p);
      p_LmFree(p,currRing);
    }
  }
  if ((lazyReduce & KSTD_NF_LAZY)==0)
  {
    for (i=strat->sl; i>=0; i--)
      pNorm(strat->S[i]);
  }
  /*- puts the elements of S also to T -*/
  for (i=0; i<=strat->sl; i++)
  {
    h.p = strat->S[i];
    h.ecart = strat->ecartS[i];
    if (strat->sevS[i] == 0) strat->sevS[i] = pGetShortExpVector(h.p);
    else assume(strat->sevS[i] == pGetShortExpVector(h.p));
    h.length = pLength(h.p);
    h.sev = strat->sevS[i];
    h.SetpFDeg();
    enterT(h,strat);
  }
  /*- compute------------------------------------------- -*/
  p = pCopy(q);
  deleteHC(&p,&o,&j,strat);
  if (TEST_OPT_PROT) { PrintS("r"); mflush(); }
  if (p!=NULL) p = redMoraNF(p,strat, lazyReduce & KSTD_NF_ECART);
  if ((p!=NULL)&&((lazyReduce & KSTD_NF_LAZY)==0))
  {
    if (TEST_OPT_PROT) { PrintS("t"); mflush(); }
    p = redtail(p,strat->sl,strat);
  }
  /*- release temp data------------------------------- -*/
  cleanT(strat);
  omFreeSize((ADDRESS)strat->T,strat->tmax*sizeof(TObject));
  omFreeSize((ADDRESS)strat->ecartS,IDELEMS(strat->Shdl)*sizeof(int));
  omFreeSize((ADDRESS)strat->sevS,IDELEMS(strat->Shdl)*sizeof(unsigned long));
  omFreeSize((ADDRESS)strat->NotUsedAxis,(pVariables+1)*sizeof(BOOLEAN));
  omfree(strat->sevT);
  omfree(strat->S_2_R);
  omfree(strat->R);

  if ((Q!=NULL)&&(strat->fromQ!=NULL))
  {
    i=((IDELEMS(Q)+IDELEMS(F)+15)/16)*16;
    omFreeSize((ADDRESS)strat->fromQ,i*sizeof(int));
    strat->fromQ=NULL;
  }
  pDelete(&strat->kHEdge);
  pDelete(&strat->kNoether);
  if ((TEST_OPT_WEIGHTM)&&(F!=NULL))
  {
    pRestoreDegProcs(pFDegOld, pLDegOld);
    if (ecartWeights)
    {
      omFreeSize((ADDRESS *)&ecartWeights,(pVariables+1)*sizeof(short));
      ecartWeights=NULL;
    }
  }
  idDelete(&strat->Shdl);
  test=save_test;
  if (TEST_OPT_PROT) PrintLn();
  return p;
}

ideal kNF1 (ideal F,ideal Q,ideal q, kStrategy strat, int lazyReduce)
{
  poly   p;
  int   i;
  int   j;
  int   o;
  LObject   h;
  ideal res;
  BITSET save_test=test;

  if (idIs0(q)) return idInit(IDELEMS(q),q->rank);
  if ((idIs0(F))&&(Q==NULL))
    return idCopy(q); /*F=0*/
  strat->ak = si_max(idRankFreeModule(F),idRankFreeModule(q));
  /*- creating temp data structures------------------- -*/
  strat->kHEdgeFound = ppNoether != NULL;
  strat->kNoether=pCopy(ppNoether);
  test|=Sy_bit(OPT_REDTAIL);
  if (TEST_OPT_STAIRCASEBOUND
  && (0<Kstd1_deg)
  && ((!strat->kHEdgeFound)
    ||(TEST_OPT_DEGBOUND && (pWTotaldegree(strat->kNoether)<Kstd1_deg))))
  {
    pDelete(&strat->kNoether);
    strat->kNoether=pOne();
    pSetExp(strat->kNoether,1, Kstd1_deg+1);
    pSetm(strat->kNoether);
    strat->kHEdgeFound=TRUE;
  }
  initBuchMoraCrit(strat);
  initBuchMoraPos(strat);
  initMora(F,strat);
  strat->enterS = enterSMoraNF;
  /*- set T -*/
  strat->tl = -1;
  strat->tmax = setmaxT;
  strat->T = initT();
  strat->R = initR();
  strat->sevT = initsevT();
  /*- set S -*/
  strat->sl = -1;
  /*- init local data struct.-------------------------- -*/
  /*Shdl=*/initS(F,Q,strat);
  if ((strat->ak!=0)
  && (strat->kHEdgeFound))
  {
    if (strat->ak!=1)
    {
      pSetComp(strat->kNoether,1);
      pSetmComp(strat->kNoether);
      poly p=pHead(strat->kNoether);
      pSetComp(p,strat->ak);
      pSetmComp(p);
      p=pAdd(strat->kNoether,p);
      strat->kNoether=pNext(p);
      p_LmFree(p,currRing);
    }
  }
  if (TEST_OPT_INTSTRATEGY && ((lazyReduce & KSTD_NF_LAZY)==0))
  {
    for (i=strat->sl; i>=0; i--)
      pNorm(strat->S[i]);
  }
  /*- compute------------------------------------------- -*/
  res=idInit(IDELEMS(q),q->rank);
  for (i=0; i<IDELEMS(q); i++)
  {
    if (q->m[i]!=NULL)
    {
      p = pCopy(q->m[i]);
      deleteHC(&p,&o,&j,strat);
      if (p!=NULL)
      {
        /*- puts the elements of S also to T -*/
        for (j=0; j<=strat->sl; j++)
        {
          h.p = strat->S[j];
          h.ecart = strat->ecartS[j];
          h.pLength = h.length = pLength(h.p);
          if (strat->sevS[j] == 0) strat->sevS[j] = pGetShortExpVector(h.p);
          else assume(strat->sevS[j] == pGetShortExpVector(h.p));
          h.sev = strat->sevS[j];
          h.SetpFDeg();
          enterT(h,strat);
        }
        if (TEST_OPT_PROT) { PrintS("r"); mflush(); }
        p = redMoraNF(p,strat, lazyReduce & KSTD_NF_ECART);
        if ((p!=NULL)&&((lazyReduce & KSTD_NF_LAZY)==0))
        {
          if (TEST_OPT_PROT) { PrintS("t"); mflush(); }
          p = redtail(p,strat->sl,strat);
        }
        cleanT(strat);
      }
      res->m[i]=p;
    }
    //else
    //  res->m[i]=NULL;
  }
  /*- release temp data------------------------------- -*/
  omFreeSize((ADDRESS)strat->T,strat->tmax*sizeof(TObject));
  omFreeSize((ADDRESS)strat->ecartS,IDELEMS(strat->Shdl)*sizeof(int));
  omFreeSize((ADDRESS)strat->sevS,IDELEMS(strat->Shdl)*sizeof(unsigned long));
  omFreeSize((ADDRESS)strat->NotUsedAxis,(pVariables+1)*sizeof(BOOLEAN));
  omfree(strat->sevT);
  omfree(strat->S_2_R);
  omfree(strat->R);
  if ((Q!=NULL)&&(strat->fromQ!=NULL))
  {
    i=((IDELEMS(Q)+IDELEMS(F)+15)/16)*16;
    omFreeSize((ADDRESS)strat->fromQ,i*sizeof(int));
    strat->fromQ=NULL;
  }
  pDelete(&strat->kHEdge);
  pDelete(&strat->kNoether);
  if ((TEST_OPT_WEIGHTM)&&(F!=NULL))
  {
    pFDeg=pFDegOld;
    pLDeg=pLDegOld;
    if (ecartWeights)
    {
      omFreeSize((ADDRESS *)&ecartWeights,(pVariables+1)*sizeof(short));
      ecartWeights=NULL;
    }
  }
  idDelete(&strat->Shdl);
  test=save_test;
  if (TEST_OPT_PROT) PrintLn();
  return res;
}

pFDegProc pFDegOld;
pLDegProc pLDegOld;
intvec * kModW, * kHomW;

long kModDeg(poly p, ring r)
{
  long o=pWDegree(p, r);
  long i=p_GetComp(p, r);
  if (i==0) return o;
  assume((i>0) && (i<=kModW->length()));
  return o+(*kModW)[i-1];
}
long kHomModDeg(poly p, ring r)
{
  int i;
  long j=0;

  for (i=r->N;i>0;i--)
    j+=p_GetExp(p,i,r)*(*kHomW)[i-1];
  if (kModW == NULL) return j;
  i = p_GetComp(p,r);
  if (i==0) return j;
  return j+(*kModW)[i-1];
}

ideal kStd(ideal F, ideal Q, tHomog h,intvec ** w, intvec *hilb,int syzComp,
          int newIdeal, intvec *vw)
{
  ideal r;
  BOOLEAN b=pLexOrder,toReset=FALSE;
  BOOLEAN delete_w=(w==NULL);
  kStrategy strat=new skStrategy;

  if(!TEST_OPT_RETURN_SB)
    strat->syzComp = syzComp;
  if (TEST_OPT_SB_1)
    strat->newIdeal = newIdeal;
  if (rField_has_simple_inverse())
    strat->LazyPass=20;
  else
    strat->LazyPass=2;
  strat->LazyDegree = 1;
  strat->ak = idRankFreeModule(F);
  strat->kModW=kModW=NULL;
  strat->kHomW=kHomW=NULL;
  if (vw != NULL)
  {
    pLexOrder=FALSE;
    strat->kHomW=kHomW=vw;
    pFDegOld = pFDeg;
    pLDegOld = pLDeg;
    pSetDegProcs(kHomModDeg);
    toReset = TRUE;
  }
  if (h==testHomog)
  {
    if (strat->ak == 0)
    {
      h = (tHomog)idHomIdeal(F,Q);
      w=NULL;
    }
    else if (!TEST_OPT_DEGBOUND)
    {
      h = (tHomog)idHomModule(F,Q,w);
    }
  }
  pLexOrder=b;
  if (h==isHomog)
  {
    if (strat->ak > 0 && (w!=NULL) && (*w!=NULL))
    {
      strat->kModW = kModW = *w;
      if (vw == NULL)
      {
        pFDegOld = pFDeg;
        pLDegOld = pLDeg;
        pSetDegProcs(kModDeg);
        toReset = TRUE;
      }
    }
    pLexOrder = TRUE;
    if (hilb==NULL) strat->LazyPass*=2;
  }
  strat->homog=h;
#ifdef KDEBUG
  idTest(F);
#endif
#ifdef HAVE_PLURAL
  if (rIsPluralRing(currRing))
  {
    if (w!=NULL)
      r = nc_GB(F, Q, *w, hilb, strat);
    else
      r = nc_GB(F, Q, NULL, hilb, strat);
  }
  else
#endif
  {
    if (pOrdSgn==-1)
    {
      if (w!=NULL)
        r=mora(F,Q,*w,hilb,strat);
      else
        r=mora(F,Q,NULL,hilb,strat);
    }
    else
    {
      if (w!=NULL)
        r=bba(F,Q,*w,hilb,strat);
      else
        r=bba(F,Q,NULL,hilb,strat);
    }
  }
#ifdef KDEBUG
  idTest(r);
#endif
  if (toReset)
  {
    kModW = NULL;
    pRestoreDegProcs(pFDegOld, pLDegOld);
  }
  pLexOrder = b;
//Print("%d reductions canceled \n",strat->cel);
  HCord=strat->HCord;
  delete(strat);
  if ((delete_w)&&(w!=NULL)&&(*w!=NULL)) delete *w;
  return r;
}

ideal kStdShift(ideal F, ideal Q, tHomog h,intvec ** w, intvec *hilb,int syzComp,
		int newIdeal, intvec *vw, int uptodeg, int lV)
{
  ideal r;
  BOOLEAN b=pLexOrder,toReset=FALSE;
  BOOLEAN delete_w=(w==NULL);
  kStrategy strat=new skStrategy;

  if(!TEST_OPT_RETURN_SB)
    strat->syzComp = syzComp;
  if (TEST_OPT_SB_1)
    strat->newIdeal = newIdeal;
  if (rField_has_simple_inverse())
    strat->LazyPass=20;
  else
    strat->LazyPass=2;
  strat->LazyDegree = 1;
  strat->ak = idRankFreeModule(F);
  strat->kModW=kModW=NULL;
  strat->kHomW=kHomW=NULL;
  if (vw != NULL)
  {
    pLexOrder=FALSE;
    strat->kHomW=kHomW=vw;
    pFDegOld = pFDeg;
    pLDegOld = pLDeg;
    pSetDegProcs(kHomModDeg);
    toReset = TRUE;
  }
  if (h==testHomog)
  {
    if (strat->ak == 0)
    {
      h = (tHomog)idHomIdeal(F,Q);
      w=NULL;
    }
    else if (!TEST_OPT_DEGBOUND)
    {
      h = (tHomog)idHomModule(F,Q,w);
    }
  }
  pLexOrder=b;
  if (h==isHomog)
  {
    if (strat->ak > 0 && (w!=NULL) && (*w!=NULL))
    {
      strat->kModW = kModW = *w;
      if (vw == NULL)
      {
        pFDegOld = pFDeg;
        pLDegOld = pLDeg;
        pSetDegProcs(kModDeg);
        toReset = TRUE;
      }
    }
    pLexOrder = TRUE;
    if (hilb==NULL) strat->LazyPass*=2;
  }
  strat->homog=h;
#ifdef KDEBUG
  idTest(F);
#endif
  if (pOrdSgn==-1)
  {
    /* error: no local ord yet with shifts */
    Print("No local ordering possible for shifts");
    return(NULL);
  }
  else
  {
    /* global ordering */
    if (w!=NULL)
      r=bbaShift(F,Q,*w,hilb,strat,uptodeg,lV);
    else
      r=bbaShift(F,Q,NULL,hilb,strat,uptodeg,lV);
  }
#ifdef KDEBUG
  idTest(r);
#endif
  if (toReset)
  {
    kModW = NULL;
    pRestoreDegProcs(pFDegOld, pLDegOld);
  }
  pLexOrder = b;
//Print("%d reductions canceled \n",strat->cel);
  HCord=strat->HCord;
  delete(strat);
  if ((delete_w)&&(w!=NULL)&&(*w!=NULL)) delete *w;
  return r;
}

//##############################################################
//##############################################################
//##############################################################
//##############################################################
//##############################################################

ideal kMin_std(ideal F, ideal Q, tHomog h,intvec ** w, ideal &M, intvec *hilb,
              int syzComp, int reduced)
{
  ideal r=NULL;
  int Kstd1_OldDeg = Kstd1_deg,i;
  intvec* temp_w=NULL;
  BOOLEAN b=pLexOrder,toReset=FALSE;
  BOOLEAN delete_w=(w==NULL);
  BOOLEAN oldDegBound=TEST_OPT_DEGBOUND;
  kStrategy strat=new skStrategy;

  if(!TEST_OPT_RETURN_SB)
     strat->syzComp = syzComp;
  if (rField_has_simple_inverse())
    strat->LazyPass=20;
  else
    strat->LazyPass=2;
  strat->LazyDegree = 1;
  strat->minim=(reduced % 2)+1;
  strat->ak = idRankFreeModule(F);
  if (delete_w)
  {
    temp_w=new intvec((strat->ak)+1);
    w = &temp_w;
  }
  if ((h==testHomog)
  )
  {
    if (strat->ak == 0)
    {
      h = (tHomog)idHomIdeal(F,Q);
      w=NULL;
    }
    else
    {
      h = (tHomog)idHomModule(F,Q,w);
    }
  }
  if (h==isHomog)
  {
    if (strat->ak > 0 && (w!=NULL) && (*w!=NULL))
    {
      kModW = *w;
      strat->kModW = *w;
      assume(pFDeg != NULL && pLDeg != NULL);
      pFDegOld = pFDeg;
      pLDegOld = pLDeg;
      pSetDegProcs(kModDeg);

      toReset = TRUE;
      if (reduced>1)
      {
        Kstd1_OldDeg=Kstd1_deg;
        Kstd1_deg = -1;
        for (i=IDELEMS(F)-1;i>=0;i--)
        {
          if ((F->m[i]!=NULL) && (pFDeg(F->m[i],currRing)>=Kstd1_deg))
            Kstd1_deg = pFDeg(F->m[i],currRing)+1;
        }
      }
    }
    pLexOrder = TRUE;
    strat->LazyPass*=2;
  }
  strat->homog=h;
  if (pOrdSgn==-1)
  {
    if (w!=NULL)
      r=mora(F,Q,*w,hilb,strat);
    else
      r=mora(F,Q,NULL,hilb,strat);
  }
  else
  {
    if (w!=NULL)
      r=bba(F,Q,*w,hilb,strat);
    else
      r=bba(F,Q,NULL,hilb,strat);
  }
#ifdef KDEBUG
  {
    int i;
    for (i=IDELEMS(r)-1; i>=0; i--) pTest(r->m[i]);
  }
#endif
  idSkipZeroes(r);
  if (toReset)
  {
    pRestoreDegProcs(pFDegOld, pLDegOld);
    kModW = NULL;
  }
  pLexOrder = b;
  HCord=strat->HCord;
  if ((delete_w)&&(temp_w!=NULL)) delete temp_w;
  if ((IDELEMS(r)==1) && (r->m[0]!=NULL) && pIsConstant(r->m[0]) && (strat->ak==0))
  {
    M=idInit(1,F->rank);
    M->m[0]=pOne();
    //if (strat->ak!=0) { pSetComp(M->m[0],strat->ak); pSetmComp(M->m[0]); }
    if (strat->M!=NULL) idDelete(&strat->M);
  }
  else
  if (strat->M==NULL)
  {
    M=idInit(1,F->rank);
    Warn("no minimal generating set computed");
  }
  else
  {
    idSkipZeroes(strat->M);
    M=strat->M;
  }
  delete(strat);
  if (reduced>2)
  {
    Kstd1_deg=Kstd1_OldDeg;
    if (!oldDegBound)
      test &= ~Sy_bit(OPT_DEGBOUND);
  }
  return r;
}

poly kNF(ideal F, ideal Q, poly p,int syzComp, int lazyReduce)
{
  if (p==NULL)
     return NULL;
  kStrategy strat=new skStrategy;
  strat->syzComp = syzComp;
  if (pOrdSgn==-1)
    p=kNF1(F,Q,p,strat,lazyReduce);
  else
    p=kNF2(F,Q,p,strat,lazyReduce);
  delete(strat);
  return p;
}

ideal kNF(ideal F, ideal Q, ideal p,int syzComp,int lazyReduce)
{
  ideal res;
  if (TEST_OPT_PROT)
  {
    Print("(S:%d)",IDELEMS(p));mflush();
  }
  kStrategy strat=new skStrategy;
  strat->syzComp = syzComp;
  if (pOrdSgn==-1)
    res=kNF1(F,Q,p,strat,lazyReduce);
  else
    res=kNF2(F,Q,p,strat,lazyReduce);
  delete(strat);
  return res;
}

/*2
*interreduces F
*/
#if 0
// new version
ideal kInterRed (ideal F, ideal Q)
{
  int   srmax,lrmax, red_result = 1;
  int   olddeg,reduc,j;
  BOOLEAN need_update=FALSE;

  kStrategy strat = new skStrategy;
  strat->kHEdgeFound = ppNoether != NULL;
  strat->kNoether=pCopy(ppNoether);
  strat->ak = idRankFreeModule(F);
  initBuchMoraCrit(strat);
  initBuchMoraPos(strat);
  strat->NotUsedAxis = (BOOLEAN *)omAlloc((pVariables+1)*sizeof(BOOLEAN));
  for (j=pVariables; j>0; j--) strat->NotUsedAxis[j] = TRUE;
  strat->enterS      = enterSBba;
  strat->posInT      = posInT17;
  strat->initEcart   = initEcartNormal;
  strat->sl          = -1;
  strat->tl          = -1;
  strat->Ll          = -1;
  strat->tmax        = setmaxT;
  strat->Lmax        = setmaxL;
  strat->T           = initT();
  strat->R           = initR();
  strat->L           = initL();
  strat->sevT        = initsevT();
  strat->red         = redLazy;
#ifdef HAVE_RINGS  //TODO Oliver
  if (rField_is_Ring(currRing)) {
    strat->red = redRing;
  }
#endif
  strat->tailRing    = currRing;
  if (pOrdSgn == -1)
    strat->honey = TRUE;
  initSL(F,Q,strat);
  for(j=strat->Ll; j>=0; j--)
    strat->L[j].tailRing=currRing;
  if (TEST_OPT_REDSB)
    strat->noTailReduction=FALSE;

  srmax = strat->sl;
  reduc = olddeg = lrmax = 0;

#ifndef NO_BUCKETS
  if (!TEST_OPT_NOT_BUCKETS)
    strat->use_buckets = 1;
#endif

  // strat->posInT = posInT_pLength;
  kTest_TS(strat);

  /* compute------------------------------------------------------- */
  while (strat->Ll >= 0)
  {
    if (strat->Ll > lrmax) lrmax =strat->Ll;/*stat.*/
    #ifdef KDEBUG
    if (TEST_OPT_DEBUG) messageSets(strat);
    #endif
    if (strat->Ll== 0) strat->interpt=TRUE;
    /* picks the last element from the lazyset L */
    strat->P = strat->L[strat->Ll];
    strat->Ll--;

    // for input polys, prepare reduction
    strat->P.PrepareRed(strat->use_buckets);

    if (strat->P.p == NULL && strat->P.t_p == NULL)
    {
      red_result = 0;
    }
    else
    {
      if (TEST_OPT_PROT)
        message((strat->honey ? strat->P.ecart : 0) + strat->P.pFDeg(),
                &olddeg,&reduc,strat, red_result);

      /* reduction of the element choosen from L */
      red_result = strat->red(&strat->P,strat);
    }

    // reduction to non-zero new poly
    if (red_result == 1)
    {
      /* statistic */
      if (TEST_OPT_PROT) PrintS("s");

      // get the polynomial (canonicalize bucket, make sure P.p is set)
      strat->P.GetP(strat->lmBin);

      int pos=posInS(strat,strat->sl,strat->P.p,strat->P.ecart);

      // reduce the tail and normalize poly
      if (TEST_OPT_INTSTRATEGY)
      {
        strat->P.pCleardenom();
        if ((TEST_OPT_REDSB)||(TEST_OPT_REDTAIL))
        {
          strat->P.p = redtailBba(&(strat->P),pos-1,strat, FALSE);
          strat->P.pCleardenom();
        }
      }
      else
      {
        strat->P.pNorm();
        if ((TEST_OPT_REDSB)||(TEST_OPT_REDTAIL))
          strat->P.p = redtailBba(&(strat->P),pos-1,strat, FALSE);
      }

#ifdef KDEBUG
      if (TEST_OPT_DEBUG){PrintS("new s:");strat->P.wrp();PrintLn();}
#endif

      // enter into S, L, and T
      enterT(strat->P, strat);
      // posInS only depends on the leading term
      strat->enterS(strat->P, pos, strat, strat->tl);
      if (pos<strat->sl)
      {
        need_update=TRUE;
#if 0
        LObject h;
        for(j=strat->sl;j>pos;j--)
        {
          if (TEST_OPT_PROT) { PrintS("+"); mflush(); }
          memset(&h, 0, sizeof(h));
          h.p=strat->S[j];
          int i;
          for(i=strat->tl;i>=0;i--)
          {
            if (pLmCmp(h.p,strat->T[i].p)==0)
            {
              if (i < strat->tl)
              {
#ifdef ENTER_USE_MEMMOVE
                memmove(&(strat->T[i]), &(strat->T[i+1]),
                   (strat->tl-i)*sizeof(TObject));
                memmove(&(strat->sevT[i]), &(strat->sevT[i+1]),
                   (strat->tl-i)*sizeof(unsigned long));
#endif
                for (int l=i; l<strat->tl; l++)
                {
#ifndef ENTER_USE_MEMMOVE
                  strat->T[l] = strat->T[l+1];
                  strat->sevT[l] = strat->sevT[l+1];
#endif
                  strat->R[strat->T[l].i_r] = &(strat->T[l]);
                }
              }
              strat->tl--;
              break;
            }
          }
          strat->S[j]=NULL;
          strat->sl--;
          if (TEST_OPT_INTSTRATEGY)
          {
            //pContent(h.p);
            h.pCleardenom(); // also does a pContent
          }
          else
          {
            h.pNorm();
          }
          strat->initEcart(&h);
                    if (strat->Ll==-1)
            pos =0;
          else
            pos = strat->posInL(strat->L,strat->Ll,&h,strat);
          h.sev = pGetShortExpVector(h.p);
          enterL(&strat->L,&strat->Ll,&strat->Lmax,h,pos);
        }
#endif
      }
      if (strat->P.lcm!=NULL) pLmFree(strat->P.lcm);
      if (strat->sl>srmax) srmax = strat->sl;
    }
#ifdef KDEBUG
    memset(&(strat->P), 0, sizeof(strat->P));
#endif
    kTest_TS(strat);
  }
#ifdef KDEBUG
  if (TEST_OPT_DEBUG) messageSets(strat);
#endif
  /* complete reduction of the standard basis--------- */
  if (TEST_OPT_REDSB)
  {
    completeReduce(strat);
    if (strat->completeReduce_retry)
    {
      // completeReduce needed larger exponents, retry
      // to reduce with S (instead of T)
      // and in currRing (instead of strat->tailRing)
      cleanT(strat);strat->tailRing=currRing;
      int i;
      for(i=strat->sl;i>=0;i--) strat->S_2_R[i]=-1;
      completeReduce(strat);
    }
  }
  else if (TEST_OPT_PROT) PrintLn();

  /* release temp data-------------------------------- */
  if (TEST_OPT_PROT) messageStat(srmax,lrmax,0,strat);
  if (Q!=NULL) updateResult(strat->Shdl,Q,strat);
  ideal shdl=strat->Shdl;
  idSkipZeroes(shdl);
  pDelete(&strat->kHEdge);
  omFreeSize((ADDRESS)strat->T,strat->tmax*sizeof(TObject));
  omFreeSize((ADDRESS)strat->ecartS,IDELEMS(strat->Shdl)*sizeof(int));
  omFreeSize((ADDRESS)strat->sevS,IDELEMS(strat->Shdl)*sizeof(unsigned long));
  omFreeSize((ADDRESS)strat->NotUsedAxis,(pVariables+1)*sizeof(BOOLEAN));
  omFreeSize((ADDRESS)strat->L,(strat->Lmax)*sizeof(LObject));
  omfree(strat->sevT);
  omfree(strat->S_2_R);
  omfree(strat->R);

  if (strat->fromQ)
  {
    for (j=IDELEMS(strat->Shdl)-1;j>=0;j--)
    {
      if(strat->fromQ[j]) pDelete(&strat->Shdl->m[j]);
    }
    omFreeSize((ADDRESS)strat->fromQ,IDELEMS(strat->Shdl)*sizeof(int));
    strat->fromQ=NULL;
  }
  delete(strat);
#if 0
  if (need_update)
  {
    ideal res=kInterRed(shdl,Q);
    idDelete(&shdl);
    shdl=res;
  }
#endif
  return shdl;
}
#else
// old version
ideal kInterRed (ideal F, ideal Q)
{
  int j;
  kStrategy strat = new skStrategy;

  ideal tempF = F;
  ideal tempQ = Q;

#ifdef HAVE_PLURAL
  if(rIsSCA(currRing))
  {
    const unsigned int m_iFirstAltVar = scaFirstAltVar(currRing);
    const unsigned int m_iLastAltVar  = scaLastAltVar(currRing);
    tempF = id_KillSquares(F, m_iFirstAltVar, m_iLastAltVar, currRing);

    // this should be done on the upper level!!! :
    //    tempQ = currRing->nc->SCAQuotient();

    if(Q == currQuotient)
      tempQ = currRing->nc->SCAQuotient();
  }
#endif

//  if (TEST_OPT_PROT)
//  {
//    writeTime("start InterRed:");
//    mflush();
//  }
  //strat->syzComp     = 0;
  strat->kHEdgeFound = ppNoether != NULL;
  strat->kNoether=pCopy(ppNoether);
  strat->ak = idRankFreeModule(tempF);
  initBuchMoraCrit(strat);
  strat->NotUsedAxis = (BOOLEAN *)omAlloc((pVariables+1)*sizeof(BOOLEAN));
  for (j=pVariables; j>0; j--) strat->NotUsedAxis[j] = TRUE;
  strat->enterS      = enterSBba;
  strat->posInT      = posInT17;
  strat->initEcart   = initEcartNormal;
  strat->sl   = -1;
  strat->tl          = -1;
  strat->tmax        = setmaxT;
  strat->T           = initT();
  strat->R           = initR();
  strat->sevT        = initsevT();
  if (pOrdSgn == -1)   strat->honey = TRUE;
  initS(tempF, tempQ, strat);
  if (TEST_OPT_REDSB)
    strat->noTailReduction=FALSE;
  updateS(TRUE,strat);
  if (TEST_OPT_REDSB && TEST_OPT_INTSTRATEGY)
    completeReduce(strat);
  //else if (TEST_OPT_PROT) PrintLn();
  pDelete(&strat->kHEdge);
  omFreeSize((ADDRESS)strat->T,strat->tmax*sizeof(TObject));
  omFreeSize((ADDRESS)strat->ecartS,IDELEMS(strat->Shdl)*sizeof(int));
  omFreeSize((ADDRESS)strat->sevS,IDELEMS(strat->Shdl)*sizeof(unsigned long));
  omFreeSize((ADDRESS)strat->NotUsedAxis,(pVariables+1)*sizeof(BOOLEAN));
  omfree(strat->sevT);
  omfree(strat->S_2_R);
  omfree(strat->R);

  if (strat->fromQ)
  {
    for (j=IDELEMS(strat->Shdl)-1;j>=0;j--)
    {
      if(strat->fromQ[j]) pDelete(&strat->Shdl->m[j]);
    }
    omFreeSize((ADDRESS)strat->fromQ,IDELEMS(strat->Shdl)*sizeof(int));
  }
//  if (TEST_OPT_PROT)
//  {
//    writeTime("end Interred:");
//    mflush();
//  }
  ideal shdl=strat->Shdl;
  idSkipZeroes(shdl);
  if (strat->fromQ)
  {
    strat->fromQ=NULL;
    ideal res=kInterRed(shdl,NULL);
    idDelete(&shdl);
    shdl=res;
  }
  delete(strat);

#ifdef HAVE_PLURAL
  if( tempF != F )
    id_Delete( &tempF, currRing);
#endif

  return shdl;
}
#endif

// returns TRUE if mora should use buckets, false otherwise
static BOOLEAN kMoraUseBucket(kStrategy strat)
{
#ifdef MORA_USE_BUCKETS
  if (TEST_OPT_NOT_BUCKETS)
    return FALSE;
  if (strat->red == redFirst)
  {
#ifdef NO_LDEG
    if (strat->syzComp==0)
      return TRUE;
#else
    if ((strat->homog || strat->honey) && (strat->syzComp==0))
      return TRUE;
#endif
  }
  else
  {
    assume(strat->red == redEcart);
    if (strat->honey && (strat->syzComp==0))
      return TRUE;
  }
#endif
  return FALSE;
}
