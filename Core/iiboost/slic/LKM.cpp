// LKM.cpp: implementation of the LKM class.
//
// Copyright (C) Radhakrishna Achanta
// All rights reserved
// Email: firstname.lastname@epfl.ch
//////////////////////////////////////////////////////////////////////

#ifdef WINDOWS
#include "stdafx.h"
#endif
#include <cfloat>
#include <cmath>
#include <iostream>
#include <fstream>
#include "LKM.h"

#ifdef _OPENMP
#include "omp.h"
#endif

#ifndef WINDOWS
#include <assert.h>
#include <slic/utils.h>
#include <string.h>

#define _ASSERT assert

#endif

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define _MAX_FNAME 200

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LKM::LKM(bool _freeMem)
{
  freeMem = _freeMem;
  m_lvec = NULL;
  m_avec = NULL;
  m_bvec = NULL;

  m_lvecvec = NULL;
  m_avecvec = NULL;
  m_bvecvec = NULL;
}

LKM::~LKM()
{
  if(freeMem)
    {
	if(m_lvec) delete [] m_lvec;
	if(m_avec) delete [] m_avec;
	if(m_bvec) delete [] m_bvec;

#if 0 //m_lvecvec is assigned to ubuffvec so don't delete it!
	if(m_lvecvec)
	{
                //for( int d = 0; d < m_depth; d++ ) delete [] m_lvecvec[d];
                delete[] m_lvecvec;
        }
#endif

	if(m_avecvec)
	{
		for( int d = 0; d < m_depth; d++ ) delete [] m_avecvec[d];
		delete [] m_avecvec;
	}
	if(m_bvecvec)
	{
		for( int d = 0; d < m_depth; d++ ) delete [] m_bvecvec[d];
		delete [] m_bvecvec;
	}
    }
}

//===========================================================================
///	RGB2LAB
///
///	http://www.filtermeister.com/codelibrary/code/rgb_lab_rgb.txt
//===========================================================================
void LKM::RGB2LAB(const int& r, const int& g, const int& b, FloatType& lval, FloatType& aval, FloatType& bval)
{
	FloatType xVal = 0.412453 * r + 0.357580 * g + 0.180423 * b;
	FloatType yVal = 0.212671 * r + 0.715160 * g + 0.072169 * b;
	FloatType zVal = 0.019334 * r + 0.119193 * g + 0.950227 * b;

	xVal /= (255.0 * 0.950456);
	yVal /=  255.0;
	zVal /= (255.0 * 1.088754);

	FloatType fY, fZ, fX;
	FloatType lVal, aVal, bVal;

	if (yVal > 0.008856f)
	{
		fY = pow(yVal, 1.0 / 3.0);
		lVal = 116.0 * fY - 16.0;
	}
	else
	{
		fY = 7.787 * yVal + 16.0 / 116.0;
		lVal = 903.3 * yVal;
	}

	if (xVal > 0.008856)
		fX = pow(xVal, 1.0 / 3.0);
	else
		fX = 7.787 * xVal + 16.0 / 116.0;

	if (zVal > 0.008856)
		fZ = pow(zVal, 1.0 / 3.0);
	else
		fZ = 7.787 * zVal + 16.0 / 116.0;

	aVal = 500.0 * (fX - fY)+128.0;
	bVal = 200.0 * (fY - fZ)+128.0;

	lval = lVal;
	aval = aVal;
	bval = bVal;
}

//===========================================================================
///	DoRGBtoLABConversion
///
///	For whole image: overlaoded floating point version
//===========================================================================
void LKM::DoRGBtoLABConversion(
	UINT*&				ubuff,
	FloatType*&					lvec,
	FloatType*&					avec,
	FloatType*&					bvec)
{
	int sz = m_width*m_height;
	lvec = new FloatType[sz];
	avec = new FloatType[sz];
	bvec = new FloatType[sz];

	for( int j = 0; j < sz; j++ )
	{
		int r = (ubuff[j] >> 16) & 0xFF;
		int g = (ubuff[j] >>  8) & 0xFF;
		int b = (ubuff[j]      ) & 0xFF;

		RGB2LAB( r, g, b, lvec[j], avec[j], bvec[j] );
	}
}

//===========================================================================
///	DoRGBtoLABConversion
///
/// For whole volume
//===========================================================================
void LKM::DoRGBtoLABConversion(
	UINT**&				ubuff,
	FloatType**&					lvec,
	FloatType**&					avec,
	FloatType**&					bvec)
{
	int sz = m_width*m_height;
	for( int d = 0; d < m_depth; d++ )
	{
		for( int j = 0; j < sz; j++ )
		{
			int r = (ubuff[d][j] >> 16) & 0xFF;
			int g = (ubuff[d][j] >>  8) & 0xFF;
			int b = (ubuff[d][j]      ) & 0xFF;

			RGB2LAB( r, g, b, lvec[d][j], avec[d][j], bvec[d][j] );
		}
	}
}

//==============================================================================
///	DetectLabEdges
//==============================================================================
void LKM::DetectLabEdges(
	FloatType*&				lvec,
	FloatType*&				avec,
	FloatType*&				bvec,
	int&					width,
	int&					height,
	vector<FloatType>&				edges)
{
	int sz = width*height;

	edges.resize(sz,0);
	for( int j = 1; j < height-1; j++ )
	{
		for( int k = 1; k < width-1; k++ )
		{
			int i = j*width+k;

			FloatType dx = (lvec[i-1]-lvec[i+1])*(lvec[i-1]-lvec[i+1]) +
						(avec[i-1]-avec[i+1])*(avec[i-1]-avec[i+1]) +
						(bvec[i-1]-bvec[i+1])*(bvec[i-1]-bvec[i+1]);

			FloatType dy = (lvec[i-width]-lvec[i+width])*(lvec[i-width]-lvec[i+width]) +
						(avec[i-width]-avec[i+width])*(avec[i-width]-avec[i+width]) +
						(bvec[i-width]-bvec[i+width])*(bvec[i-width]-bvec[i+width]);

			edges[i] = (dx + dy);
		}
	}
}

//===========================================================================
///	PerturbSeeds
//===========================================================================
void LKM::PerturbSeeds(
	vector<FloatType>&				kseedsl,
	vector<FloatType>&				kseedsa,
	vector<FloatType>&				kseedsb,
	vector<FloatType>&				kseedsx,
	vector<FloatType>&				kseedsy)
{
	vector<FloatType> edges(0);
	DetectLabEdges(m_lvec, m_avec, m_bvec, m_width, m_height, edges);

	const int dx8[8] = {-1, -1,  0,  1, 1, 1, 0, -1};
	const int dy8[8] = { 0, -1, -1, -1, 0, 1, 1,  1};
	
	int numseeds = kseedsl.size();

	for( int n = 0; n < numseeds; n++ )
	{
		int ox = kseedsx[n];//original x
		int oy = kseedsy[n];//original y
		int oind = oy*m_width + ox;

		int storeind = oind;
		for( int i = 0; i < 8; i++ )
		{
			int nx = ox+dx8[i];//new x
			int ny = oy+dy8[i];//new y

			if( nx >= 0 && nx < m_width && ny >= 0 && ny < m_height)
			{
				int nind = ny*m_width + nx;
				if( edges[nind] < edges[storeind])
				{
					storeind = nind;
				}
			}
		}
		if(storeind != oind)
		{
			kseedsx[n] = storeind%m_width;
			kseedsy[n] = storeind/m_width;
			kseedsl[n] = m_lvec[storeind];
			kseedsa[n] = m_avec[storeind];
			kseedsb[n] = m_bvec[storeind];
		}
	}
}


//===========================================================================
///	GetKValues_LABXY
///
/// The k seed values are taken as uniform spatial pixel samples.
//===========================================================================
void LKM::GetKValues_LABXY(
	vector<FloatType>&				kseedsl,
	vector<FloatType>&				kseedsa,
	vector<FloatType>&				kseedsb,
	vector<FloatType>&				kseedsx,
	vector<FloatType>&				kseedsy,
	const int&					STEP,
	const bool&					perturbseeds)
{
	int numseeds(0);
	int n(0);

	//int xstrips = m_width/STEP;
	//int ystrips = m_height/STEP;
	int xstrips = (0.5+FloatType(m_width)/FloatType(STEP));
	int ystrips = (0.5+FloatType(m_height)/FloatType(STEP));

	int xerr = m_width  - STEP*xstrips;
	int yerr = m_height - STEP*ystrips;

	FloatType xerrperstrip = FloatType(xerr)/FloatType(xstrips);
	FloatType yerrperstrip = FloatType(yerr)/FloatType(ystrips);

	int xoff = STEP/2;
	int yoff = STEP/2;
	//-------------------------
	numseeds = xstrips*ystrips;
	//-------------------------
	kseedsl.resize(numseeds);
	kseedsa.resize(numseeds);
	kseedsb.resize(numseeds);
	kseedsx.resize(numseeds);
	kseedsy.resize(numseeds);

	for( int y = 0; y < ystrips; y++ )
	{
		int ye = y*yerrperstrip;
		for( int x = 0; x < xstrips; x++ )
		{
			int xe = x*xerrperstrip;
			int i = (y*STEP+yoff+ye)*m_width + (x*STEP+xoff+xe);

			//_ASSERT(n < numseeds);
			
			kseedsl[n] = m_lvec[i];
			kseedsa[n] = m_avec[i];
			kseedsb[n] = m_bvec[i];
			kseedsx[n] = (x*STEP+xoff+xe);
			kseedsy[n] = (y*STEP+yoff+ye);
			n++;
		}
	}

	if(perturbseeds)
	{
		PerturbSeeds(kseedsl, kseedsa, kseedsb, kseedsx, kseedsy);
	}
}

#if 0
//===========================================================================
///	GetKValues_LABXYZ
///
/// The k seed values are taken as uniform spatial pixel samples.
//===========================================================================
void LKM::GetKValues_LABXYZ(
	vector<FloatType>&				kseedsl,
	vector<FloatType>&				kseedsa,
	vector<FloatType>&				kseedsb,
	vector<FloatType>&				kseedsx,
	vector<FloatType>&				kseedsy,
	vector<FloatType>&				kseedsz,
	const int&					STEP)
{
	int numseeds(0);
	int n(0);

	int xstrips = (0.5+FloatType(m_width)/FloatType(STEP));
	int ystrips = (0.5+FloatType(m_height)/FloatType(STEP));
	int zstrips = (0.5+FloatType(m_depth)/FloatType(STEP));

	int xerr = m_width  - STEP*xstrips;
	int yerr = m_height - STEP*ystrips;
	int zerr = m_depth  - STEP*zstrips;

	FloatType xerrperstrip = FloatType(xerr)/FloatType(xstrips);
	FloatType yerrperstrip = FloatType(yerr)/FloatType(ystrips);
	FloatType zerrperstrip = FloatType(zerr)/FloatType(zstrips);

	int xoff = STEP/2;
	int yoff = STEP/2;
	int zoff = STEP/2;
	//-------------------------
	numseeds = xstrips*ystrips*zstrips;
	//-------------------------
	kseedsl.resize(numseeds);
	kseedsa.resize(numseeds);
	kseedsb.resize(numseeds);
	kseedsx.resize(numseeds);
	kseedsy.resize(numseeds);
	kseedsz.resize(numseeds);

	for( int z = 0; z < zstrips; z++ )
	{
		int ze = z*zerrperstrip;
		int d = (z*STEP+zoff+ze);
		for( int y = 0; y < ystrips; y++ )
		{
			int ye = y*yerrperstrip;
			for( int x = 0; x < xstrips; x++ )
			{
				int xe = x*xerrperstrip;
				int i = (y*STEP+yoff+ye)*m_width + (x*STEP+xoff+xe);

				//_ASSERT(n < numseeds);
				
				kseedsl[n] = m_lvecvec[d][i];
				kseedsa[n] = m_avecvec[d][i];
				kseedsb[n] = m_bvecvec[d][i];
				kseedsx[n] = (x*STEP+xoff+xe);
				kseedsy[n] = (y*STEP+yoff+ye);
				kseedsz[n] = d;
				n++;
			}
		}
	}
}
#endif
//===========================================================================
///	GetKValues_LABXYZ
///
/// The k seed values are taken as uniform spatial pixel samples.
//===========================================================================
void LKM::GetKValues_LABXYZ(
	vector<FloatType>&				kseedsl,
	vector<FloatType>&				kseedsx,
	vector<FloatType>&				kseedsy,
	vector<FloatType>&				kseedsz,
	const int&					STEP,
    int                   zStep  // optional, if <= 0, then zStep = STEP
                           )
{
    if (zStep <= 0)
        zStep = STEP;
    
	int numseeds(0);
	int n(0);
        int sz = m_width*m_height;

	int xstrips = (0.5+FloatType(m_width)/FloatType(STEP));
	int ystrips = (0.5+FloatType(m_height)/FloatType(STEP));
	int zstrips = (0.5+FloatType(m_depth)/FloatType(zStep));

	int xerr = m_width  - STEP*xstrips;
	int yerr = m_height - STEP*ystrips;
	int zerr = m_depth  - zStep*zstrips;

	FloatType xerrperstrip = FloatType(xerr)/FloatType(xstrips);
	FloatType yerrperstrip = FloatType(yerr)/FloatType(ystrips);
	FloatType zerrperstrip = FloatType(zerr)/FloatType(zstrips);

	int xoff = STEP/2;
	int yoff = STEP/2;
	int zoff = zStep/2;
	//-------------------------
	numseeds = xstrips*ystrips*zstrips;
	//-------------------------
	kseedsl.resize(numseeds);
	kseedsx.resize(numseeds);
	kseedsy.resize(numseeds);
	kseedsz.resize(numseeds);

	for( int z = 0; z < zstrips; z++ )
	{
		int ze = z*zerrperstrip;
		int d = (z*zStep+zoff+ze);
                int zOff = z*sz;

		for( int y = 0; y < ystrips; y++ )
		{
			int ye = y*yerrperstrip;
			for( int x = 0; x < xstrips; x++ )
			{
				int xe = x*xerrperstrip;
                                int i = (y*STEP+yoff+ye)*m_width + (x*STEP+xoff+xe) + zOff;

				//_ASSERT(n < numseeds);
				
                                kseedsl[n] = m_lvecvec[i];
				kseedsx[n] = (x*STEP+xoff+xe);
				kseedsy[n] = (y*STEP+yoff+ye);
				kseedsz[n] = d;
				n++;
			}
		}
	}
}

//===========================================================================
///	PerformLKMClustering
///
///	Performs k mean segmentation. It is fast because it looks locally, not
/// over the entire image.
//===========================================================================
void LKM::PerformLKMClustering(
	vector<FloatType>&		kseedsl,
	vector<FloatType>&		kseedsa,
	vector<FloatType>&		kseedsb,
	vector<FloatType>&		kseedsx,
	vector<FloatType>&		kseedsy,
	sidType*&	   	klabels,
	const int&	       	STEP,
        const float M)
{
	int sz = m_width*m_height;
	const int numk = kseedsl.size();
	int numitr(0);

	//----------------
	int offset = STEP;
	//----------------

	
	vector<FloatType> clustersize(numk, 0);
	vector<FloatType> inv(numk, 0);//to store 1/clustersize[k] values

	vector<FloatType> sigmal(numk, 0);
	vector<FloatType> sigmaa(numk, 0);
	vector<FloatType> sigmab(numk, 0);
	vector<FloatType> sigmax(numk, 0);
	vector<FloatType> sigmay(numk, 0);
	vector<FloatType> distvec(sz, DBL_MAX);

	//FloatType invwt = 1.0/((STEP/10.0)*(STEP/10.0));
        FloatType invwt = 1.0/((STEP/M)*(STEP/M));

	int x1, y1, x2, y2;
	FloatType l, a, b;
	FloatType dist;
	FloatType distxy;
	for( int itr = 0; itr < 10; itr++ )
	{
		distvec.assign(sz, DBL_MAX);
		for( int n = 0; n < numk; n++ )
		{
                  /*
			y1 = max(0,			kseedsy[n]-offset);
			y2 = min(m_height,	kseedsy[n]+offset);
			x1 = max(0,			kseedsx[n]-offset);
			x2 = min(m_width,	kseedsx[n]+offset);
                  */
                  y1 = max((FloatType)0.0,			kseedsy[n]-offset);
                  y2 = min((FloatType)m_height,	kseedsy[n]+offset);
                  x1 = max((FloatType)0.0,			kseedsx[n]-offset);
                  x2 = min((FloatType)m_width,	kseedsx[n]+offset);


			for( int y = y1; y < y2; y++ )
			{
				for( int x = x1; x < x2; x++ )
				{
					int i = y*m_width + x;
					//_ASSERT( y < m_height && x < m_width && y >= 0 && x >= 0 );

					l = m_lvec[i];
					a = m_avec[i];
					b = m_bvec[i];

					dist =			(l - kseedsl[n])*(l - kseedsl[n]) +
									(a - kseedsa[n])*(a - kseedsa[n]) +
									(b - kseedsb[n])*(b - kseedsb[n]);

					distxy =		(x - kseedsx[n])*(x - kseedsx[n]) +
									(y - kseedsy[n])*(y - kseedsy[n]);
					//------------------------------------------------------------------------
					//dist += distxy;//not to be used...works best only for 10x10 superpixels
					//dist *= (20.0*log10(distxy));//not so cool....very irregular superpixels
					//------------------------------------------------------------------------
					//dist *= distxy;// very cool![SOLUTION 1]i.e. MULTIPLICATION
					//dist = sqrt(dist*distxy);//[same as SOLUTION 1]
					//------------------------------------------------------------------------
					dist += distxy*invwt;//quite cool[SOLUTION 2]i.e ADDITION
					//dist = sqrt(dist) + sqrt(distxy*invwt);[almost same as SOLUTION 2]
					//------------------------------------------------------------------------
					//dist += 20.0*log10(distxy);//[SOLUTION 3]...can generate some ugly superpixels,
												//but usually decent ones. Too many noisy segments
												//RemoveSmallSegments is not used. Probably discard...
					//------------------------------------------------------------------------
					if( dist < distvec[i] )
					{
						distvec[i] = dist;
						klabels[i]  = n;
					}
				}
			}
		}
		//-----------------------------------------------------------------
		// Recalculate the centroid and store in the seed values
		//-----------------------------------------------------------------
		//instead of reassigning memory on each iteration, just reset.
	
		sigmal.assign(numk, 0);
		sigmaa.assign(numk, 0);
		sigmab.assign(numk, 0);
		sigmax.assign(numk, 0);
		sigmay.assign(numk, 0);
		clustersize.assign(numk, 0);

		{int ind(0);
		for( int r = 0; r < m_height; r++ )
		{
			for( int c = 0; c < m_width; c++ )
			{
				sigmal[klabels[ind]] += m_lvec[ind];
				sigmaa[klabels[ind]] += m_avec[ind];
				sigmab[klabels[ind]] += m_bvec[ind];
				sigmax[klabels[ind]] += c;
				sigmay[klabels[ind]] += r;

				clustersize[klabels[ind]] += 1.0;
				ind++;
			}
		}}

		{for( int k = 0; k < numk; k++ )
		{
			if( clustersize[k] <= 0 ) clustersize[k] = 1;
			inv[k] = 1.0/clustersize[k];//computing inverse now to multiply, than divide later
		}}
		
		{for( int k = 0; k < numk; k++ )
		{
			kseedsl[k] = sigmal[k]*inv[k];
			kseedsa[k] = sigmaa[k]*inv[k];
			kseedsb[k] = sigmab[k]*inv[k];
			kseedsx[k] = sigmax[k]*inv[k];
			kseedsy[k] = sigmay[k]*inv[k];
		}}
	}
}

#if 0
//===========================================================================
///	PerformLKMVoxelClustering
///
///	Performs k mean segmentation. It is fast because it looks locally, not
/// over the entire image.
//===========================================================================
void LKM::PerformLKMVoxelClustering(
	vector<FloatType>&				kseedsl,
	vector<FloatType>&				kseedsa,
	vector<FloatType>&				kseedsb,
	vector<FloatType>&				kseedsx,
	vector<FloatType>&				kseedsy,
	vector<FloatType>&				kseedsz,
	sidType**&    	       			klabels,
	const int&	       			STEP,
        const FloatType cubeness)
{
	int sz = m_width*m_height;
	const int numk = kseedsl.size();
	int numitr(0);

        if(numk >= MAX_SID)
          {
            printf("[LKM] Error : numk=%d >= MAX_SID=%d\n",numk,MAX_SID);
            exit(-1);
          }

	//----------------
	int offset = STEP;
	//----------------

	vector<FloatType> clustersize(numk, 0);
	vector<FloatType> inv(numk, 0);//to store 1/clustersize[k] values

	vector<FloatType> sigmal(numk, 0);
	vector<FloatType> sigmaa(numk, 0);
	vector<FloatType> sigmab(numk, 0);
	vector<FloatType> sigmax(numk, 0);
	vector<FloatType> sigmay(numk, 0);
	vector<FloatType> sigmaz(numk, 0);

	vector< FloatType > initdouble(sz, DBL_MAX);
	vector< vector<FloatType> > distvec(m_depth, initdouble);
	//vector<double> distvec(sz, DBL_MAX);

	//double invwt = 1.0/((STEP/20.0)*(STEP/20.0));
        FloatType invwt = 1.0/((STEP/cubeness)*(STEP/cubeness));

	int x1, y1, x2, y2, z1, z2;
	FloatType l, a, b;
	FloatType dist;
	FloatType distxyz;
	for( int itr = 0; itr < 5; itr++ )
	{
		distvec.assign(m_depth, initdouble);
		for( int n = 0; n < numk; n++ )
		{
                  /*
			y1 = max(0,			kseedsy[n]-offset);
			y2 = min(m_height,	kseedsy[n]+offset);
			x1 = max(0,			kseedsx[n]-offset);
			x2 = min(m_width,	kseedsx[n]+offset);
			z1 = max(0,			kseedsz[n]-offset);
			z2 = min(m_depth,	kseedsz[n]+offset);
                  */
			y1 = max(0.0,			kseedsy[n]-offset);
			y2 = min((FloatType)m_height,	kseedsy[n]+offset);
			x1 = max(0.0,			kseedsx[n]-offset);
			x2 = min((FloatType)m_width,	kseedsx[n]+offset);
			z1 = max(0.0,			kseedsz[n]-offset);
			z2 = min((FloatType)m_depth,	kseedsz[n]+offset);

			for( int z = z1; z < z2; z++ )
			{
				for( int y = y1; y < y2; y++ )
				{
					for( int x = x1; x < x2; x++ )
					{
						int i = y*m_width + x;
						//_ASSERT( y < m_height && x < m_width && y >= 0 && x >= 0 );

						l = m_lvecvec[z][i];
						a = m_avecvec[z][i];
						b = m_bvecvec[z][i];

						dist =			(l - kseedsl[n])*(l - kseedsl[n]) +
										(a - kseedsa[n])*(a - kseedsa[n]) +
										(b - kseedsb[n])*(b - kseedsb[n]);

						distxyz =		(x - kseedsx[n])*(x - kseedsx[n]) +
										(y - kseedsy[n])*(y - kseedsy[n]) +
										(z - kseedsz[n])*(z - kseedsz[n]);
						//------------------------------------------------------------------------
						//dist += distxy;//not to be used...works best only for 10x10 superpixels
						//dist *= (20.0*log10(distxy));//not so cool....very irregular superpixels
						//------------------------------------------------------------------------
						//dist *= distxy;// very cool![SOLUTION 1]i.e. MULTIPLICATION
						//dist = sqrt(dist*distxy);//[same as SOLUTION 1]
						//------------------------------------------------------------------------
						dist += distxyz*invwt;//quite cool[SOLUTION 2]i.e ADDITION
						//dist = sqrt(dist) + sqrt(distxy*invwt);[almost same as SOLUTION 2]
						//------------------------------------------------------------------------
						//dist += 20.0*log10(distxy);//[SOLUTION 3]...can generate some ugly superpixels,
													//but usually decent ones. Too many noisy segments
													//RemoveSmallSegments is not used. Probably discard...
						//------------------------------------------------------------------------
						if( dist < distvec[z][i] )
						{
							distvec[z][i] = dist;
							klabels[z][i]  = n;
						}
					}
				}
			}
		}
		//-----------------------------------------------------------------
		// Recalculate the centroid and store in the seed values
		//-----------------------------------------------------------------
		//instead of reassigning memory on each iteration, just reset.
	
		sigmal.assign(numk, 0);
		sigmaa.assign(numk, 0);
		sigmab.assign(numk, 0);
		sigmax.assign(numk, 0);
		sigmay.assign(numk, 0);
		sigmaz.assign(numk, 0);
		clustersize.assign(numk, 0);

		for( int d = 0; d < m_depth; d++  )
		{
			int ind(0);
			for( int r = 0; r < m_height; r++ )
			{
				for( int c = 0; c < m_width; c++ )
				{
					sigmal[klabels[d][ind]] += m_lvecvec[d][ind];
					sigmaa[klabels[d][ind]] += m_avecvec[d][ind];
					sigmab[klabels[d][ind]] += m_bvecvec[d][ind];
					sigmax[klabels[d][ind]] += c;
					sigmay[klabels[d][ind]] += r;
					sigmaz[klabels[d][ind]] += d;

					clustersize[klabels[d][ind]] += 1.0;
					ind++;
				}
			}
		}

		{for( int k = 0; k < numk; k++ )
		{
			if( clustersize[k] <= 0 ) clustersize[k] = 1;
			inv[k] = 1.0/clustersize[k];//computing inverse now to multiply, than divide later
		}}
		
		{for( int k = 0; k < numk; k++ )
		{
			kseedsl[k] = sigmal[k]*inv[k];
			kseedsa[k] = sigmaa[k]*inv[k];
			kseedsb[k] = sigmab[k]*inv[k];
			kseedsx[k] = sigmax[k]*inv[k];
			kseedsy[k] = sigmay[k]*inv[k];
			kseedsz[k] = sigmaz[k]*inv[k];
		}}
	}
}
#endif

//===========================================================================
///	PerformLKMVoxelClustering
///
///	Performs k mean segmentation. It is fast because it looks locally, not
/// over the entire image.
//===========================================================================
void LKM::PerformLKMVoxelClustering(
	vector<FloatType>&				kseedsl,
	vector<FloatType>&				kseedsx,
	vector<FloatType>&				kseedsy,
	vector<FloatType>&				kseedsz,
	sidType**&				klabels,
	const int&			      	STEP,
        const FloatType cubeness,
        int zStep
                                   )
{
    if (zStep <= 0)
        zStep = STEP;
    
	int sz = m_width*m_height;

	const int numk = kseedsl.size();
	int numitr(0);

        if(numk >= MAX_SID)
          {
            printf("[LKM] Error : numk >= MAX_SID\n");
            exit(-1);
          }

	//----------------
	int offset = STEP;
    int zOffset = zStep;
	//----------------

	vector<FloatType> clustersize(numk, 0);
	//vector<FloatType> inv(numk, 0);//to store 1/clustersize[k] values

	vector<FloatType> sigmal(numk, 0);
	vector<FloatType> sigmax(numk, 0);
	vector<FloatType> sigmay(numk, 0);
	vector<FloatType> sigmaz(numk, 0);

	vector< FloatType > initdouble(sz, DBL_MAX);
	vector< vector<FloatType> > distvec(m_depth, initdouble);
	//vector<double> distvec(sz, DBL_MAX);

	//double invwt = 1.0/((STEP/20.0)*(STEP/20.0));
	FloatType invwt = 1.0/((STEP/cubeness)*(STEP/cubeness));
	
	for( int itr = 0; itr < 5; itr++ )
	{
		distvec.assign(m_depth, initdouble);
		
		for( int n = 0; n < numk; n++ )
		{
                  /*
			y1 = max(0,			kseedsy[n]-offset);
			y2 = min(m_height,	kseedsy[n]+offset);
			x1 = max(0,			kseedsx[n]-offset);
			x2 = min(m_width,	kseedsx[n]+offset);
			z1 = max(0,			kseedsz[n]-offset);
			z2 = min(m_depth,	kseedsz[n]+offset);
                  */
			int y1 = max((FloatType)0.0,			kseedsy[n]-offset);
			int y2 = min((FloatType)m_height,	kseedsy[n]+offset);
			int x1 = max((FloatType)0.0,			kseedsx[n]-offset);
			int x2 = min((FloatType)m_width,	kseedsx[n]+offset);
			int z1 = max((FloatType)0.0,			kseedsz[n]-zOffset);
			int z2 = min((FloatType)m_depth,	kseedsz[n]+zOffset);
			
			//#pragma omp parallel for schedule(dynamic)
			for( int z = z1; z < z2; z++ )
			{
				int zOff = z*sz;
				for( int y = y1; y < y2; y++ )
				{
					for( int x = x1; x < x2; x++ )
					{
						int ixy = y*m_width + x;
						int i = ixy + zOff;
						//_ASSERT( y < m_height && x < m_width && y >= 0 && x >= 0 );

						FloatType l = m_lvecvec[i];

						FloatType dist = (l - kseedsl[n])*(l - kseedsl[n]);

						FloatType distxyz = (x - kseedsx[n])*(x - kseedsx[n]) +
                                                  (y - kseedsy[n])*(y - kseedsy[n]) +
                                                  (z - kseedsz[n])*(z - kseedsz[n]);
						//------------------------------------------------------------------------
						//dist += distxy;//not to be used...works best only for 10x10 superpixels
						//dist *= (20.0*log10(distxy));//not so cool....very irregular superpixels
						//------------------------------------------------------------------------
						//dist *= distxy;// very cool![SOLUTION 1]i.e. MULTIPLICATION
						//dist = sqrt(dist*distxy);//[same as SOLUTION 1]
						//------------------------------------------------------------------------
						dist += distxyz*invwt;//quite cool[SOLUTION 2]i.e ADDITION
						//dist = sqrt(dist) + sqrt(distxy*invwt);[almost same as SOLUTION 2]
						//------------------------------------------------------------------------
						//dist += 20.0*log10(distxy);//[SOLUTION 3]...can generate some ugly superpixels,
													//but usually decent ones. Too many noisy segments
													//RemoveSmallSegments is not used. Probably discard...
						//------------------------------------------------------------------------

						if( dist < distvec[z][ixy] )
						{
							distvec[z][ixy] = dist;
							klabels[z][ixy]  = n;
						}
					}
				}
			}
		}
		//-----------------------------------------------------------------
		// Recalculate the centroid and store in the seed values
		//-----------------------------------------------------------------
		//instead of reassigning memory on each iteration, just reset.
	
		sigmal.assign(numk, 0);
		sigmax.assign(numk, 0);
		sigmay.assign(numk, 0);
		sigmaz.assign(numk, 0);
		clustersize.assign(numk, 0);

		for( int d = 0; d < m_depth; d++  )
		{
			int ind(0);
                        int zOff = d*sz;
			for( int r = 0; r < m_height; r++ )
			{
				for( int c = 0; c < m_width; c++ )
				{
                                        sigmal[klabels[d][ind]] += m_lvecvec[ind + zOff];
					sigmax[klabels[d][ind]] += c;
					sigmay[klabels[d][ind]] += r;
					sigmaz[klabels[d][ind]] += d;

					clustersize[klabels[d][ind]] += 1.0;
					ind++;
				}
			}
		}
/*
		{for( int k = 0; k < numk; k++ )
		{
			if( clustersize[k] <= 0 ) clustersize[k] = 1;
			inv[k] = 1.0/clustersize[k];//computing inverse now to multiply, than divide later
		}}*/
		
		//#pragma omp parallel for
		for( int k = 0; k < numk; k++ )
		{
			FloatType cSize = clustersize[k];
			if( cSize <= 0 ) cSize = 1;
			FloatType invK = 1.0 / cSize;
			
			kseedsl[k] = sigmal[k]*invK;
			kseedsx[k] = sigmax[k]*invK;
			kseedsy[k] = sigmay[k]*invK;
			kseedsz[k] = sigmaz[k]*invK;
		}
	}
}

//===========================================================================
///	EnforceConnectivityForLargeImages
//===========================================================================
void LKM::EnforceConnectivityForLargeImages(
	const int					width,
	const int					height,
	sidType*&		       			labels,//input labels that need to be corrected to remove stray single labels
	int&						numlabels)
{
	const int dx8[8] = {-1, -1,  0,  1, 1, 1, 0, -1};
	const int dy8[8] = { 0, -1, -1, -1, 0, 1, 1,  1};

	for( int j = 1; j < height-1; j++ )
	{
		for( int k = 1; k < width-1; k++ )
		{
			int oi = j*width+k;
			int np(0);
			int count(0);
			for( int i = 0; i < 8; i++ )
			{
				int x = k + dx8[i];
				int y = j + dy8[i];

				//if( (x >= 0 && x < width) && (y >= 0 && y < height) )
				{
					int ni = y*width + x;

					if( labels[oi] != labels[ni] )
					{
						count++;
					}
					else break;
				}
			}
			if( count > 7 )
			{
				labels[oi] = labels[oi-1];
			}
		}
	}
	CountAndRelabel(labels, numlabels);

	//-------------------------------------------------
	// The following lines of code are for verification
	//-------------------------------------------------
	//int sz = width*height;
	//vector<int> compsz(numlabels, 0);
	//{for( int i = 0; i < sz; i++ )
	//{
	//	compsz[labels[i]]++;
	//}}
	//for( int n = 0; n < numlabels; n++ )
	//{
	//	_ASSERT(compsz[n] > 1);
	//}
}

//===========================================================================
///	EnforceConnectivityForLargeImages
//===========================================================================
void LKM::EnforceConnectivityForLargeImages(
	const int&					width,
	const int&					height,
	const int&					depth,
	sidType**&			       		labels,//input labels that need to be corrected to remove stray single labels
	int&						numlabels)
{
	//const int dx8[8] = {-1, -1,  0,  1, 1, 1, 0, -1};
	//const int dy8[8] = { 0, -1, -1, -1, 0, 1, 1,  1};
	//const int dz3[3] = { -1. 0, 1 

	for( int d = 1; d < depth-1; d++ )
	{
		for( int j = 1; j < height-1; j++ )
		{
			for( int k = 1; k < width-1; k++ )
			{
				int oi = j*width+k;
				int np(0);
				int count(0);
				for( int nd = d-1; nd <= d+1; nd++ )
				{
					for( int y = j-1; y <= j+1; y++ )
					{
						for( int x = k-1; x <= k+1; x++ )
						{
							int ni = y*width + x;

							if( labels[d][oi] != labels[nd][ni] )
							{
								count++;
							}
							else break;
						}
					}
				}
				if( count > 25 )
				{
					labels[d][oi] = labels[d][oi-1];
				}
			}
		}
	}
	CountAndRelabel(labels, numlabels);

	//-------------------------------------------------
	// The following lines of code are for verification
	//-------------------------------------------------
	//int sz = width*height;
	//vector<int> compsz(numlabels, 0);
	//{for( int i = 0; i < sz; i++ )
	//{
	//	compsz[labels[i]]++;
	//}}
	//for( int n = 0; n < numlabels; n++ )
	//{
	//	_ASSERT(compsz[n] > 1);
	//}
}


//===========================================================================
///	SaveLabels
///
///	Save labels in raster scan order.
//===========================================================================
void LKM::SaveLabels(
	sidType*&					labels,
	const int					width,
	const int					height,
	string				filename,
	string				path) 
{
	int sz = width*height;

        /*
	char* fname = new char[_MAX_FNAME];
	char* extn = new char[_MAX_FNAME];
#ifdef WINDOWS
  _splitpath(filename.c_str(), NULL, NULL, fname, extn);
#else
  splitpath(filename, fname, extn);
#endif
	string temp = fname;
	string finalpath = path + temp + string(".dat");
        */

	ofstream outfile;
	string finalpath = path + filename;
        printf("finalpath : %s\n", finalpath.c_str());
	outfile.open(finalpath.c_str(), ios::binary);
	for( int i = 0; i < sz; i++ )
	{
		outfile.write((const char*)&labels[i], sizeof(sidType));
	}
	outfile.close();

        /*
        delete[] fname;
        delete[] extn;
        */
}


//===========================================================================
///	SaveLabels
///
///	Save labels in raster scan order.
//===========================================================================
void LKM::SaveLabels_Text(
                          sidType*&					labels,
                          const int					width,
                          const int					height,
                          string				filename,
                          string				path) 
{
	int sz = width*height;

        /*
	char* fname = new char[_MAX_FNAME];
	char* extn = new char[_MAX_FNAME];
#ifdef WINDOWS
  _splitpath(filename.c_str(), NULL, NULL, fname, extn);
#else
  splitpath(filename, fname, extn);
#endif
	string temp = fname;
	string finalpath = path + temp + string(".dat");
        */

	ofstream outfile;
	string finalpath = path + filename;
        printf("finalpath : %s\n", finalpath.c_str());
	outfile.open(finalpath.c_str());
	//for( int i = 0; i < sz; i++ )
        int i = 0;
        for( int h = 0; h < height; h++ )
          {
            for( int w = 0; w < width -1; w++ )
              {
                outfile << labels[i] << " ";
                i++;
              }
            outfile << labels[i] << endl;
            i++;
          }
	outfile.close();

        /*
        delete[] fname;
        delete[] extn;
        */
}


//===========================================================================
///	SaveLabels
///
///	Save labels in raster scan order.
//===========================================================================
void LKM::SaveLabels(
	const sidType**&				labels,
	const int&					width,
	const int&					height,
	const int&					depth,
	const string&				filename,
	const string&				path) 
{
	int sz = width*height;

	char fname[_MAX_FNAME];
	char extn[_MAX_FNAME];
#ifdef WINDOWS
  _splitpath(filename.c_str(), NULL, NULL, fname, extn);
#else
  splitpath(filename, fname, extn);
#endif
	string temp = fname;

	ofstream outfile;
	string finalpath = path + temp + string(".dat");
	outfile.open(finalpath.c_str(), ios::binary);
	for( int d = 0; d < depth; d++ )
	{
		for( int i = 0; i < sz; i++ )
		{
			outfile.write((const char*)&labels[d][i], sizeof(sidType));
		}
	}
	outfile.close();
}

//===========================================================================
///	DoSegmentation_LABXY
///
/// There is option to save the labels if needed. However the filename and
/// path need to be provided.
//===========================================================================
void LKM::DoSuperpixelSegmentation(
	UINT*&				ubuff,
	const int					width,
	const int					height,
	sidType*&	       				klabels,
	int&						numlabels,
	const int&					STEP,
        const float M)
{
	vector<FloatType> kseedsl(0);
	vector<FloatType> kseedsa(0);
	vector<FloatType> kseedsb(0);
	vector<FloatType> kseedsx(0);
	vector<FloatType> kseedsy(0);

	//--------------------------------------------------
	m_width  = width;
	m_height = height;
	int sz = m_width*m_height;
	//klabels.resize( sz, -1 );
	//--------------------------------------------------
	klabels = new sidType[sz];
	for( int s = 0; s < sz; s++ ) klabels[s] = UNDEFINED_LABEL;
	//--------------------------------------------------
	DoRGBtoLABConversion(ubuff, m_lvec, m_avec, m_bvec);
	//--------------------------------------------------

	bool perturbseeds(true);
	GetKValues_LABXY(kseedsl, kseedsa, kseedsb, kseedsx, kseedsy, STEP, perturbseeds);

	PerformLKMClustering(kseedsl, kseedsa, kseedsb, kseedsx, kseedsy, klabels, STEP, M);
	numlabels = kseedsl.size();
	//-------------------------------------------------------------------------
	// Save the labels if needed. Provide image name and the folder to save in.
	//-------------------------------------------------------------------------
	//SaveLabels(klabels, width, height, filename, savepath);

        ///int K = (int)(((double)sz/(double)STEP*STEP)+0.5) + 2;
        int expectedSuperpixelSize = sz/numlabels; // STEP*STEP

        sidType* newlabels = new sidType[sz];
        RelabelStraySuperpixels(klabels,width,height,newlabels,numlabels,expectedSuperpixelSize);
        sidType* tempPtr = klabels;
        klabels = newlabels;
        delete[] tempPtr;
}


#if 0
//===========================================================================
///	DoSegmentation_LABXY
///
/// There is option to save the labels if needed. However the filename and
/// path need to be provided.
//===========================================================================
void LKM::DoSupervoxelSegmentation(
	UINT**&						ubuffvec,
	const int&					width,
	const int&					height,
	const int&					depth,
	sidType**&	       				klabels,
	int&						numlabels,
	const int&					STEP,
        const FloatType cubeness)
{
	vector<FloatType> kseedsl(0);
	vector<FloatType> kseedsa(0);
	vector<FloatType> kseedsb(0);
	vector<FloatType> kseedsx(0);
	vector<FloatType> kseedsy(0);
	vector<FloatType> kseedsz(0);

	//--------------------------------------------------
	m_width  = width;
	m_height = height;
	m_depth  = depth;
	int sz = m_width*m_height;
	
	//--------------------------------------------------
	klabels = new sidType*[depth];
	m_lvecvec = new FloatType*[depth];
	m_avecvec = new FloatType*[depth];
	m_bvecvec = new FloatType*[depth];
	for( int d = 0; d < depth; d++ )
	{
		klabels[d] = new sidType[sz];
		m_lvecvec[d] = new FloatType[sz];
		m_avecvec[d] = new FloatType[sz];
		m_bvecvec[d] = new FloatType[sz];
		for( int s = 0; s < sz; s++ )
		{
                  klabels[d][s] = UNDEFINED_LABEL;
		}
	}
	
	//--------------------------------------------------
	DoRGBtoLABConversion(ubuffvec, m_lvecvec, m_avecvec, m_bvecvec);
	//--------------------------------------------------

	GetKValues_LABXYZ(kseedsl, kseedsa, kseedsb, kseedsx, kseedsy, kseedsz, STEP);

	PerformLKMVoxelClustering(kseedsl, kseedsa, kseedsb, kseedsx, kseedsy, kseedsz, klabels, STEP, cubeness);
	numlabels = kseedsl.size();

        /*
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
        */

        //RelabelSupervoxels(width, height, depth, klabels, numlabels);

        RelabelStraySupervoxels(width, height, depth, klabels, numlabels, STEP);

	//-------------------------------------------------------------------------
	// Save the labels if needed. Provide image name and the folder to save in.
	//-------------------------------------------------------------------------
	//SaveLabels(klabels, width, height, filename, savepath);
}

#endif

void LKM::DoSupervoxelSegmentationForGrayVolume(
                                                const unsigned char*                            ubuffvec,
                                                const int&					width,
                                                const int&					height,
                                                const int&					depth,
                                                sidType**&					klabels,
                                                int&						numlabels,
                                                const int&					STEP,
                                                const FloatType cubeness,
                                                int zStep    // if zStep <= 0, then zStep = STEP
                                               )
{
        vector<FloatType> kseedsl(0);
        vector<FloatType> kseedsx(0);
        vector<FloatType> kseedsy(0);
        vector<FloatType> kseedsz(0);
        
        if (zStep <= 0)
            zStep = STEP;

	//--------------------------------------------------
	m_width  = width;
	m_height = height;
	m_depth  = depth;
	int sz = m_width*m_height;
	
	//--------------------------------------------------
        unsigned int memSize = sizeof(int)*depth + depth*sz*sizeof(sidType);
        printf("[LKM] memory required to run supervoxel algorithm = %dMb\n", memSize/(1024*1024));
	klabels = new sidType*[depth];
        m_lvecvec = ubuffvec;
	for( int d = 0; d < depth; d++ )
	{
		klabels[d] = new sidType[sz];
		for( int s = 0; s < sz; s++ )
		{
			klabels[d][s] = UNDEFINED_LABEL;
		}
	}
	
	//--------------------------------------------------
	//DoRGBtoLABConversion(ubuffvec, m_lvecvec, m_avecvec, m_bvecvec);
	//--------------------------------------------------

	GetKValues_LABXYZ(kseedsl, kseedsx, kseedsy, kseedsz, STEP, zStep);

	PerformLKMVoxelClustering(kseedsl, kseedsx, kseedsy, kseedsz, klabels, STEP, cubeness);
	numlabels = kseedsl.size();

        /*
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
	EnforceConnectivityForLargeImages(width, height, depth, klabels, numlabels);
        */

        //RelabelSupervoxels(width, height, depth, klabels, numlabels);

        RelabelStraySupervoxels(width, height, depth, klabels, numlabels, STEP);

	//-------------------------------------------------------------------------
	// Save the labels if needed. Provide image name and the folder to save in.
	//-------------------------------------------------------------------------
	//SaveLabels(klabels, width, height, filename, savepath);
}

//===========================================================================
///	RelabelSupervoxels
///
///	Some supervoxels may be unconnected, Relabel them. Recursive algorithm
/// used here, can crash if stack overflows. This will only happen if the
/// superpixels are very large.
//===========================================================================
void LKM::RelabelSupervoxels(
	const int&					width,
	const int&					height,
	const int&					depth,
	sidType**&		       			labels,
	int&						numlabels)
{
	int sz = width*height;
	//------------------
	// memory allocation
	//------------------
	sidType** nlabels = new sidType*[depth];
	{for( int d = 0; d < depth; d++ )
	{
		nlabels[d] = new sidType[sz];
		for( int i = 0; i < sz; i++ ) nlabels[d][i] = UNDEFINED_LABEL;
	}}
	//------------------
	// labeling
	//------------------
	sidType lab(0);
	{for( int d = 0; d < depth; d++ )
	{
		int i(0);
		for( int h = 0; h < height; h++ )
		{
			for( int w = 0; w < width; w++ )
			{
                          //if(nlabels[d][i] < 0)
                          if(nlabels[d][i] == UNDEFINED_LABEL)
				{
					nlabels[d][i] = lab;
					FindNext(labels, nlabels, depth, height, width, d, h, w, lab);
					lab++;
				}
				i++;
			}
		}
	}}
	//------------------
	// mem de-allocation
	//------------------
        /*
	{for( int d = 0; d < depth; d++ )
	{
		for( int i = 0; i < sz; i++ ) labels[d][i] = nlabels[d][i];
	}}
	{for( int d = 0; d < depth; d++ )
	{
		delete [] nlabels[d];
	}}
	delete [] nlabels;
        */
	for( int d = 0; d < depth; d++ )
	{
		delete [] labels[d];
	}
	delete [] labels;
        labels = nlabels;

	//------------------
	numlabels = lab;
	//------------------
}


//===========================================================================
///	RelabelStraySupervoxels
///
///	Some supervoxels may be unconnected, Relabel them. Recursive algorithm
/// used here, can crash if stack overflows. This will only happen if the
/// superpixels are very large.
//===========================================================================
void LKM::RelabelStraySupervoxels(
	const int&					width,
	const int&					height,
	const int&					depth,
	sidType**&		       			labels,
	int&						numlabels,
	const int&					STEP,
    int zStep
                                 )
{
    if (zStep <= 0)
        zStep = STEP;
    
	int sz = width*height;
	const int SUPSZ = STEP*STEP*zStep;

	int adjlabel(0);//adjacent label
        int* xvec = new int[SUPSZ*4*4];//a large safe size
        int* yvec = new int[SUPSZ*4*4];//a large safe size
        int* zvec = new int[SUPSZ*4*4];//a large safe size
	//------------------
	// memory allocation
	//------------------
	sidType** nlabels = new sidType*[depth];
	{for( int d = 0; d < depth; d++ )
	{
		nlabels[d] = new sidType[sz];
		for( int i = 0; i < sz; i++ ) nlabels[d][i] = UNDEFINED_LABEL;
	}}
	//------------------
	// labeling
	//------------------
	sidType lab(0);
	{for( int d = 0; d < depth; d++ )
	{
		int i(0);
		for( int h = 0; h < height; h++ )
		{
			for( int w = 0; w < width; w++ )
			{
			  //if(nlabels[d][i] < 0)
			  if(nlabels[d][i] == UNDEFINED_LABEL)
				{
					nlabels[d][i] = lab;
					//-------------------------------------------------------
					// Quickly find an adjacent label for use later if needed
					//-------------------------------------------------------
					{for( int n = 0; n < 10; n++ )
					{
						int x = w + dx10[n];
						int y = h + dy10[n];
						int z = d + dz10[n];
						if( (x >= 0 && x < width) && (y >= 0 && y < height) && (z >= 0 && z < depth) )
						{
							int nindex = y*width + x;
							//if(nlabels[z][nindex] >= 0) adjlabel = nlabels[z][nindex];
                                                        if(nlabels[z][nindex] != UNDEFINED_LABEL) adjlabel = nlabels[z][nindex];
						}
					}}
					//{for( int i = -1; i <= 1; i++ )
					//{
					//	for(int j = -1; j <= 1; j++ )
					//	{
					//		for( int k = -1; k <= 1; k++ )
					//		{
					//			int z = d+i; int y = h+j; int x = w+k;
					//			if( (x >= 0 && x < width) && (y >= 0 && y < height) && (z >= 0 && z < depth) )
					//			{
					//				int nindex = y*width + x;
					//				if(nlabels[nindex] >= 0) adjlabel = nlabels[d][nindex];
					//			}
					//		}
					//	}
					//}}
					xvec[0] = w; yvec[0] = h; zvec[0] = d;
					int count(1);
					//--------------------------------------------------------
					FindNext(labels, nlabels, depth, height, width, d, h, w, lab, xvec, yvec, zvec, count);
					//-------------------------------------------------------
					// If segment size is less then a limit, assign an
					// adjacent label found before, and decrement label count.
					//-------------------------------------------------------
					if(count <= (SUPSZ >> 2))
					{
						for( int c = 0; c < count; c++ )
						{
							int ind = yvec[c]*width+xvec[c];
							nlabels[zvec[c]][ind] = adjlabel;
						}
						lab--;
					}
					//--------------------------------------------------------
					lab++;
				}
				i++;
			}
		}
	}}
	//------------------
	// mem de-allocation
	//------------------
        /*
	{for( int d = 0; d < depth; d++ )
	{
		for( int i = 0; i < sz; i++ ) labels[d][i] = nlabels[d][i];
	}}
	{for( int d = 0; d < depth; d++ )
	{
		delete [] nlabels[d];
	}}
	delete [] nlabels;
        */

	for( int d = 0; d < depth; d++ )
	{
                delete[] labels[d];
	}
	delete [] labels;
        labels = nlabels;

	//------------------
	if(xvec) delete [] xvec;
	if(yvec) delete [] yvec;
	if(zvec) delete [] zvec;

        xvec = yvec = zvec = 0;

	//------------------
	numlabels = lab;
	//------------------
}
