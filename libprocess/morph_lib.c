/* 
   Library of mathematical morphology and tip estimation routines
   written by John Villarrubia, National Institute of Standards and
   Technology, an agency of the U.S. Department of Commerce,
   Gaithersburg, MD 20899, USA.

   The algorithms presented below are intended to be used for research
   purposes only and bear no warranty, either express or implied.
   Please note that within the United States, copyright protection,
   under Section 105 of the United States Code, Title 17, is not
   available for any work of the United States Government and/or for
   any works conceived by United States Government employees under this
   software release. User acknowledges that Villarrubia's actual work
   is in the public domain and is not subject to copyright. However, if
   User utilizes the aforementioned government-created algorithms in a
   manner which substantially alters User's work, User agrees to
   acknowledge this by reference to Villarrubia's papers, which are
   listed below.
*/


#include <stdio.h>
#include <memory.h>
#include "morph_lib.h"

#define MIN(a,b) ((a)<(b)?a:b)
#define MAX(a,b) ((a)>(b)?a:b)

/* The following routines allow allocation and freeing of matrices */

long **allocmatrix(long ysiz, long xsiz)
/* 
   Allocates an integer matrix of dimension [ysiz][xsiz] using an array
   of pointers to rows. ysiz is the number of rows. xsiz is the number
   of columns.
*/
{
   long **mptr,		/* points to allocated matrix */
       i;		/* counter */

   /* Allocate pointers to rows */
   mptr = (long **)malloc(ysiz*sizeof(long*));
   if (mptr == NULL) {
      printf("Error: Allocation of mptr failed in allocmatrix\n");
      exit(1);
   }
   

   /* Allocate rows */
   for (i=0;i<ysiz;i++) {
      mptr[i]=(long *)malloc(xsiz*sizeof(long));
      if (mptr[i] == NULL) {
         printf("Error: Allocation of mptr[%ld] failed in allocmatrix\n",i);
         exit(1);
      }
   }  
   
   /* Done. Return result. */
   return mptr;
}

void freematrix(long **mptr, long ysiz)
/* Frees memory allocated with allocmatrix */
{
   long i;
   for (i=0;i<ysiz;i++) free(mptr[i]);
   free(mptr);
}

/* The following routine performs reflection of integer arrays. The integers
   used are standard C integers, which PV-WAVE would call long integers. */

long **ireflect(long **surface, long surf_xsiz, long surf_ysiz)
{
   long **result;
   long i,j;               /* index */

   /* create output array of appropriate size */
   result = allocmatrix(surf_ysiz,surf_xsiz);

   for (j=0;j<surf_ysiz;j++) {   /* Loop over all points in output array */
      for (i=0;i<surf_xsiz;i++) {
	 result[j][i] = -surface[surf_ysiz-1-j][surf_xsiz-1-i];
      }
   }
   return(result);
}


/* The following routine performs dilation on integer arrays. The integers
   used are standard C integers (for my compiler 4-byte), which PV-WAVE
   would call long integers. */

long **idilation(long **surface, long surf_xsiz, long surf_ysiz,
	      long **tip, long tip_xsiz, long tip_ysiz, long xc, long yc)
{
   long **result;
   long i,j,px,py;               /* index */
   long max;
   long pxmin,pxmax,pymin,pymax; /* range of indices into tip */
   long temp;

   /* create output array of appropriate size */
   result = allocmatrix(surf_ysiz,surf_xsiz);

   for (j=0;j<surf_ysiz;j++) {   /* Loop over all points in output array */
      /* Compute allowed range of py. This may be different from
	 the full range of the tip due to edge overlaps. */
      pymin = MAX(j-surf_ysiz+1,-yc);
      pymax = MIN(tip_ysiz-yc-1,j);
      for (i=0;i<surf_xsiz;i++) {
	 /* Compute allowed range of px. This may be different from
	    the full range of the tip due to edge overlaps. */
	 pxmin = MAX(i-surf_xsiz+1,-xc);
	 pxmax = MIN(tip_xsiz-xc-1,i);
	 max = surface[j-pymin][i-pxmin]+tip[pymin+yc][pxmin+xc];
	 for (px=pxmin;px<=pxmax;px++) {        /* Loop over points in tip */
	    for (py=pymin;py<=pymax;py++) {
	       temp = surface[j-py][i-px]+tip[py+yc][px+xc];
	       max = MAX(temp,max);
	    }
	 }
	 result[j][i] = max;
      }
   }
   return(result);
}

long **ierosion(long **image, long im_xsiz, long im_ysiz,
	      long **tip, long tip_xsiz, long tip_ysiz,
	      long xc, long yc)
{
   long **result;
   long i,j,px,py;               /* index */
   long min;
   long pxmin,pxmax,pymin,pymax; /* range of indices into tip */
   long temp;

   /* create output array of appropriate size */
   result = allocmatrix(im_ysiz,im_xsiz);

   for (j=0;j<im_ysiz;j++) {   /* Loop over all points in output array */
      /* Compute allowed range of py. This may be different from
	 the full range of the tip due to edge overlaps. */
      pymin = MAX(-j,-yc);
      pymax = MIN(tip_ysiz-yc,im_ysiz-j)-1;
      for (i=0;i<im_xsiz;i++) {
	 /* Compute allowed range of px. This may be different from
	    the full range of the tip due to edge overlaps. */
	 pxmin = MAX(-xc,-i);
	 pxmax = MIN(tip_xsiz-xc,im_xsiz-i)-1;
	 min = image[j+pymin][i+pxmin]-tip[pymin+yc][pxmin+xc];
	 for (px=pxmin;px<=pxmax;px++) {        /* Loop over points in tip */
	    for (py=pymin;py<=pymax;py++) {
	       temp = image[j+py][i+px]-tip[py+yc][px+xc];
	       min = MIN(temp,min);
	    }
	 }
	 result[j][i] = min;
      }
   }
   return(result);
}

long **icmap(long **image, long im_xsiz, long im_ysiz,
	      long **tip, long tip_xsiz,long tip_ysiz,
              long **rsurf,
	      long xc, long yc)
{
   long **cmap;
   long imx,imy,tpx,tpy;               /* index */
   long tpxmin,tpxmax,tpymin,tpymax;
   long count;
   long rxc,ryc;			/* center coordinates of reflected tip */
   long x,y;

   rxc = tip_xsiz-1-xc;
   ryc = tip_ysiz-1-yc;

   /* create output array of appropriate size */
   cmap = allocmatrix(im_ysiz,im_xsiz);
   for (imy=0;imy<im_ysiz;imy++) for (imx=0;imx<im_xsiz;imx++) cmap[imy][imx]=0;

   /*
       Loop over all pixels in the interior of the image. We skip
       pixels near the edge. Since it is possible there are unseen
       touches over the edge, we must conservatively leave these cmap
       entries at 0.
   */
   for (imy=ryc;imy<=im_ysiz+ryc-tip_ysiz;imy++) {   
      for (imx=rxc;imx<=im_xsiz+rxc-tip_xsiz;imx++) {
         tpxmin = MAX(0,rxc-imx);
         tpxmax = MIN(tip_xsiz-1,im_xsiz-1+rxc-imx);
         tpymin = MAX(0,ryc-imy);
         tpymax = MIN(tip_ysiz-1,im_ysiz-1+ryc-imy);
         count = 0;
         for (tpy=tpymin;tpy<=tpymax&&count<2;tpy++) {
            for (tpx=tpxmin;tpx<=tpxmax&&count<2;tpx++) {
               if (image[imy][imx]-tip[tip_ysiz-1-tpy][tip_xsiz-1-tpx] == 
                  rsurf[tpy+imy-ryc][tpx+imx-rxc]) {
                  count++;	/* increment count */
                  x = tpx+imx-rxc;	/* remember coordinates */
                  y = tpy+imy-ryc;
               }
            }
         }
         if (count==1) cmap[y][x] = 1;	/* 1 contact = good recon */
      }
   }
   return(cmap);
}

long **iopen(long **image, long im_xsiz, long im_ysiz, long **tip, long tip_xsiz, long tip_ysiz)
{
   long **result,**eros;

   eros = ierosion(image,im_xsiz,im_ysiz,tip,tip_xsiz,tip_ysiz,
      tip_xsiz/2,tip_ysiz/2);
   result = idilation(eros,im_xsiz,im_ysiz,tip,tip_xsiz,tip_ysiz,
      tip_xsiz/2,tip_ysiz/2);
   freematrix(eros,im_ysiz);/* free intermediate result */
   return(result);
}

/* The following routine estimates tip size by calling tip_estimate_iter
   until it converges. */

void itip_estimate(long **image, long im_xsiz, long im_ysiz,
		   long tip_xsiz, long tip_ysiz, long xc, long yc, long **tip0,
		   long thresh)
{
   long iter=0;
   long count=1;

   while (count) {
      iter++;
      count = itip_estimate_iter(image,im_xsiz,im_ysiz,
         tip_xsiz,tip_ysiz,xc,yc,tip0,thresh);
      printf ("Finished iteration #%ld. ",iter);
      printf ("%ld image locations produced refinement.\n",count);
   }
}

/* The following routine performs one iteration of the tip estimation
   recursion algorithm. The threshold parameter is used as follows:  A
   new estimate of the tip height at a given pixel is computed
   according to the formula given by Eqn 13 (or 16) of Surf. Sci. 321
   (1994) 287. This value is then augmented by thresh. If the old
   estimate is less than the augmented new estimate, no action is taken
   Otherwise, the value is changed to agree with the augmented new
   estimate. Thus, if thresh=0, this routine implements Eqn 13
   directly. Larger values of threshold give greater noise immunity at
   the cost of degrading the estimate.

   In this version of the routine, the values of the revised estimate
   replace those in tip0, and the number of pixels where the value was
   changed is returned.
   */

#define MINUS_INF -2147483648L           /* smallest integer */
#define INFINITY 2147483647L             /* largest integer */
/* The interior of the image is easier (and safer) to use in calculating
   the tip estimate. At the edges part of the tip extends beyond the
   edge where the image values are unknown. It is possible to handle
   this case by making worst case assumptions and thereby extract some
   information even from the data near the edge. Below, I separate the code
   into two parts, one which handles the interior and one which handles
   the edges. This serves two functions. First, because the edges can
   be tricky to handle properly, I had more than the usual share of bugs
   in this part of the code during development. It was useful to be able
   to turn this part of the code on or off at will. Second: The execution
   of the routine is slightly faster with the code divided since it is not
   then necessary to do checking for special cases in the part of the code
   that handles the interior. 

*/

long itip_estimate_iter(long **image, long im_xsiz, long im_ysiz, long tip_xsiz, long tip_ysiz,
   long xc, long yc, long **tip0, long thresh)
{
   long ixp,jxp;         /* index into the image (x') */
   long **open;  

   long count=0;         /* counts places where tip estimate is improved */

   open = iopen(image,im_xsiz,im_ysiz,tip0,tip_xsiz,tip_ysiz);

   for (jxp=tip_ysiz-1-yc;jxp<=im_ysiz-1-yc;jxp++) {
      for (ixp=tip_xsiz-1-xc;ixp<=im_xsiz-1-xc;ixp++) {
         if (image[jxp][ixp]-open[jxp][ixp] > thresh)
            if (itip_estimate_point(ixp,jxp,image,im_xsiz,im_ysiz,tip_xsiz,
               tip_ysiz,xc,yc,tip0,thresh)) count++;
      }
   }

   freematrix(open,im_ysiz);
   return(count);
}

/* 
   The following is a routine to do an initial estimate of the tip
   shape by using only a few selected points within the image. If the
   points are well-chosen this can produce most of the tip shape
   refinement with a small fraction of the compute time.
*/
void itip_estimate0(long **image, long im_xsiz, long im_ysiz, long tip_xsiz, long tip_ysiz,
   long xc, long yc, long **tip0, long thresh)
{
   long i,j,n;
   long arraysize; /* size of array allocated to store list of image maxima */
   long *x,*y;	  /* point to coordinates of image maxima */
   long iter=0;
   long count;
   long delta;	/* defines what is meant by near neighborhood for purposes
		   of point selection. */
   /* 
      We need to create temporary arrays to hold a list of selected
      image coordinates.  The space needed depends upon the image. This
      creates a memory management issue. We address it by allowing for
      300 maxima--a good number (corresponding perhaps to a grainy
      surface) which should be big enough for most images. Then we
      monitor memory usage and reallocate a larger array if necessary.

      Coordinates to be used are determined by the routine useit, below,
      which returns either 1 (if the given coordinate is to be used) or
      0 (if not). The user can substitute his own version of this routine
      if he believes he has a more economical algorithm for choosing
      points.
   */
   arraysize = 300;
   x = (long *)malloc(arraysize*sizeof(long));
   if (x == NULL) {
      printf("Unable to allocate x array in itip_estimate0 routine.\n");
      return;
   }
   y = (long *)malloc(arraysize*sizeof(long));
   if (y == NULL) {
      printf("Unable to allocate y array in itip_estimate0 routine.\n");
      free(x);
      return;
   }

   /* 
   Now choose a nearest neighborhood size to send the useit routine.
   The neighborhood should be at least equal to 1 (i.e. consider all
   points with x,y within +/- 1 of the one under consideration).
   Otherwise ALL points are used, equivalent to the full tip_estimate
   routine, and no speed advantage is derived, which loses the whole
   point of having a tip_estimate0. However, the size of the
   neighborhood should in principle scale with the size of the tip. I
   use a small fraction of tip size (1/10) because in practice the
   routine seems to run acceptably quickly even at this
   setting--there's no point in sacrificing performance for speed if
   the present speed is acceptable.
   */

   delta = MAX(MAX(tip_xsiz,tip_ysiz)/10,1);

   /* Create a list of coordinates to use */
   n=0;		/* Number of image maxima found so far */
   for (j=tip_ysiz-1-yc;j<=im_ysiz-1-yc;j++) {
      for (i=tip_xsiz-1-xc;i<=im_xsiz-1-xc;i++) {
         if (useit(i,j,image,im_xsiz,im_ysiz,delta)) {
            if (n == arraysize) {   /* need more room in temporary arrays */
               arraysize *= 2;	/* increase array size by factor of 2 */
               x = (long *)realloc(x,arraysize*sizeof(long));
               if (x == NULL) {
                  printf("Unable to realloc x array in itip_estimate0.\n");
                  free(y);
                  return;
               }
               y = (long *)realloc(y,arraysize*sizeof(long));
               if (y == NULL) {
                  printf("Unable to realloc y array in itip_estimate0.\n");
                  free(x);
                  return;
               }
            }
            x[n] = i;	/* We found another one */
            y[n] = j;
            n++;
         }
      }
   }
   printf("Found %ld internal local maxima\n",n);
   

   /* Now refine tip at these coordinates recursively until no more change */
   do {
      count = 0;
      iter++;
      for (i=0;i<n;i++)
         if (itip_estimate_point(x[i],y[i],image,im_xsiz,im_ysiz,
            tip_xsiz,tip_ysiz,xc,yc,tip0,thresh)) count++;
      printf 
         ("Finished iteration #%ld. %ld image locations produced refinement.\n",
         iter,count);
   } while (count);

   /* free temporary space */
   free(x); free(y);
}

/* 
   The following is a routine that determines whether a selected point
   at coordintes x,y within an image is deemed to be suitable for
   image refinement. In this implementation, the algorithm simply
   decides to use the point if it is a local maximum of the image.
   It defines a local maximum as a point with height greater than any
   of its near neighbors, provided there are not too many near neighbors
   with values equal to the maximum (which indicates a flat).
*/

long useit(long x, long y, long **image, long sx, long sy, long delta)
{
   long xmin,xmax,ymin,ymax;	/* actual interval to search */
   long i,j;
   long max=image[y][x]; /* value of maximum height in the neighborhood */
   long count=0;	/* counts # spots where pixel = max value */

   xmin = MAX(x-delta,0);
   xmax = MIN(x+delta,sx-1);
   ymin = MAX(y-delta,0);
   ymax = MIN(y+delta,sy-1);

   for (j=ymin;j<=ymax;j++) {
      for (i=xmin;i<=xmax;i++) {
         max = image[j][i]>=max?(count++,image[j][i]):max;
      }
   }   

   /* If the point equals the maximum value in the neighborhood we use it,
      unless there are too many points in the neighborhood with the same
      property--i.e. the neighborhood is flat */
   if (max == image[y][x] && count <= ((2*delta+1)^2)/5) return(1);
   return(0);
}

/* 
   The following routine does the same thing as itip_estimate_iter, except
   that instead of looping through all i,j contained within the image, it
   computes the tip shape as deduced from a single i,j coordinate. For what
   is this useful? The order of evaluation of the points can affect the
   execution speed. That is because the image at some locations puts great
   constraints on the tip shape. If the tip shape is refined by considering
   these points first, time is saved later. The following routine, since it
   performs the calculation at a single point, allows the user to select
   the order in which image coordinates are considered. In using the 
   routine in this mode, the fact that the refined tip replaces tip0 means
   results of one step automatically become the starting point for the next
   step. The routine returns an integer which is the number of pixels 
   within the starting tip estimate which were updated.

   To compile codes which does not use parts of the image within a tipsize
   of the edge, set USE_EDGES to 0 on the next line.   
*/
	
#define USE_EDGES 1			/* set to 1 to include edges */

long itip_estimate_point(long ixp, long jxp, long **image, 
			 long im_xsiz, long im_ysiz, long tip_xsiz, long tip_ysiz,
			 long xc, long yc, long **tip0, long thresh)
{
   long ix,jx,           /* index into the output tip array (x) */
       id,jd;           /* index into p' (d) */
   long temp,imagep,
       dil;   		 /* various intermediate results */
   long count=0;         /* counts places where tip estimate is improved */
   long interior;	/* =1 if no edge effects to worry about */
   int apexstate,
       xstate,
       inside = 1,	/* Point is inside image */
       outside = 0;	/* point is outside image */


   interior = jxp>=tip_ysiz-1 && jxp<=im_ysiz-tip_ysiz && ixp>=tip_xsiz-1 && 
             ixp<=im_xsiz-tip_xsiz;


   if (interior) {
      for (jx=0;jx<tip_ysiz;jx++) {
         for (ix=0;ix<tip_xsiz;ix++) {
	    /* First handle the large middle area where we don't have to
	       be concerned with edge problems. Because edges are far
	       away, we can leave out the overhead of checking for them
	       in this section. */
	       imagep = image[jxp][ixp];
	    dil = MINUS_INF;         /* initialize maximum to -infinity */
	    for (jd=0;jd<tip_ysiz;jd++) {
	       for (id=0;id<tip_xsiz;id++) {
		  if (imagep-image[jxp+yc-jd][ixp+xc-id] > 
		     tip0[jd][id]) continue;
		  temp = image[jx+jxp-jd][ix+ixp-id]+tip0[jd][id]-imagep;
		  dil = MAX(dil,temp);
	       } /* end for id */
	    } /* end for jd */
	    if (dil == MINUS_INF) continue;
	    tip0[jx][ix] = 
	       dil<tip0[jx][ix]-thresh?(count++,dil+thresh):tip0[jx][ix];
         } /* end for ix */
      } /* end for jx */
      return(count);
   } /* endif */
#if USE_EDGES
	    /* Now handle the edges */
   for (jx=0;jx<tip_ysiz;jx++) {
      for (ix=0;ix<tip_xsiz;ix++) {
	 imagep = image[jxp][ixp];
	 dil = MINUS_INF;         /* initialize maximum to -infinity */
	 for (jd=0;jd<=tip_ysiz-1 && dil<INFINITY;jd++) {
	    for (id=0;id<=tip_xsiz-1;id++) {
               /* Determine whether the tip apex at (xc,yc) lies within
                  the domain of the translated image, and if so, if it
                  is inside (i.e. below or on the surface of) the image. */
               apexstate = outside;	/* initialize */
               if (jxp+yc-jd<0 || jxp+yc-jd>=im_ysiz ||
                   ixp+xc-id<0 || ixp+xc-id>=im_xsiz) apexstate = inside;
               else if (imagep-image[jxp+yc-jd][ixp+xc-id] <= 
	             tip0[jd][id]) apexstate = inside;
               /* Determine whether the point (ix,jx) under consideration
                  lies within the domain of the translated image */
               if (jxp+jx-jd<0 || jxp+jx-jd>=im_ysiz ||
                   ixp+ix-id<0 || ixp+ix-id>=im_xsiz) xstate = outside;
               else xstate = inside;

               /* There are 3 actions we might take, depending upon
                  which of 4 states (2 apexstate possibilities times 2 
                  xstate ones) we are in. */

               /* If apex is outside and x is either in or out no change
                  is made for this (id,jd) */
               if (apexstate==outside) continue;

               /* If apex is inside and x is outside
		  worst case is translated image value -> INFINITY.
		  This would result in no change for ANY (id,jd). We
		  therefore abort the loop and go to next (ix,jx) value */
               if (xstate==outside) goto nextx;

               /* The only remaining possibility is x and apex both inside. 
                  This is the same case we treated in the interior. */
	       temp = image[jx+jxp-jd][ix+ixp-id]+tip0[jd][id]-imagep;
               dil = MAX(dil,temp);
	    } /* end for id */
	 } /* end for jd */
	 if (dil == MINUS_INF) continue;
	
	 tip0[jx][ix] = 
	    dil<tip0[jx][ix]-thresh?(count++,dil+thresh):tip0[jx][ix];
nextx:;
      } /* end for ix */
   } /* end for jx */
#endif

   return(count);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

