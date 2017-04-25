//
//  libhedrot_calibration.c
//  libhedrot
//
//  Created by Alexis Baskind on 17/04/17.
//
//

#include "libhedrot_calibration.h"

// include cblas and lapack for matrix operations
#ifdef __MACH__  // if mach (mac os X)
#include <Accelerate/Accelerate.h>
#else
#if defined(_WIN32) || defined(_WIN64)
#include "lapacke.h"
#endif
#endif


//=====================================================================================================
// function ellipsoidFit
//=====================================================================================================
//
// find the center and raddii of a set of raw data (Nx3), assumed to be on an ellipsoid
// The calculation is done in double precision, the result is converted to single.
//
// method:
// classical ellipsoid fit algorithm without rotation = least-squares optimization on the linearized problem (after variable changes):
// a*X^2 + b*Y^2 + c*Z^2 + d*2*X + e*2*Y + f*2*Z = 1;
//
// the radii and offset are calculated afterwards as follows:
// offsets = [-d/a -e/b -f/c]
// radii = [sqrt(gamma/a) sqrt(gamma/b) sqrt(gamma/c)]
// ... with gamma = 1 + ( d^2/a + e^2/b + f^2/c );
//
// returns 1 (error) if calibration failed
int ellipsoidFit(calibrationData* calData, float* accOffset, float* accScaling) {
    int i;
    
    // definitions
#ifdef __MACH__  // if mach (mac os X)
    // constants
    __CLPK_integer one = 1, six=6;
    double rcond = 1/MAX_CONDITION_NUMBER; // reverse maximum condition number
    
    double matrixD[calData->numberOfSamples*6]; // internal input matrix A
    double matrixB[calData->numberOfSamples]; // internal input matrix B (column mit ones), output = solution
    double vectorS[6]; //output = singular values
    __CLPK_integer rank; // effective rank of the matrix
    double wkopt;
    double *work;
    __CLPK_integer n = calData->numberOfSamples;
    __CLPK_integer lda = calData->numberOfSamples, ldb = calData->numberOfSamples;
    __CLPK_integer lwork, info;
    double gamma;
#else
#if defined(_WIN32) || defined(_WIN64)
    // constants
    double rcond = 1/MAX_CONDITION_NUMBER; // reverse maximum condition number
    
    double *matrixD = (double*) malloc(calData->numberOfSamples*6*sizeof(double)); // internal input matrix A
    double *matrixB = (double*) malloc(calData->numberOfSamples*6*sizeof(double)); // internal input matrix B (column mit ones), output = solution
    double vectorS[6]; //output = singular values
    lapack_int rank; // effective rank of the matrix
    int n = calData->numberOfSamples;
    int lda = calData->numberOfSamples, ldb = calData->numberOfSamples;
    double gamma;
#endif
#endif
    
    
    // Build the matrix D (rows = X^2, Y^2, Z^2, 2*X, 2*Y, 2*Z) and the matrix ONES (N*1)
    for(i=0; i<calData->numberOfSamples; i++) {
        matrixD[0*calData->numberOfSamples+i] = calData->rawSamples[i][0] * calData->rawSamples[i][0];
        matrixD[1*calData->numberOfSamples+i] = calData->rawSamples[i][1] * calData->rawSamples[i][1];
        matrixD[2*calData->numberOfSamples+i] = calData->rawSamples[i][2] * calData->rawSamples[i][2];
        
        matrixD[3*calData->numberOfSamples+i] = 2 * calData->rawSamples[i][0];
        matrixD[4*calData->numberOfSamples+i] = 2 * calData->rawSamples[i][1];
        matrixD[5*calData->numberOfSamples+i] = 2 * calData->rawSamples[i][2];
        
        matrixB[i] = 1;
    }
    
    
#ifdef __MACH__  // if mach (mac os X)
    /* Query and allocate the optimal workspace */
    lwork = -1;
    dgelss_(&n, &six, &one, matrixD, &lda, matrixB, &ldb, vectorS, &rcond, &rank, &wkopt, &lwork, &info);
    lwork = (int)wkopt;
    work = (double*) malloc( lwork*sizeof(double) );
    /* Solve the equations A*X = B */
    dgelss_(&n, &six, &one, matrixD, &lda, matrixB, &ldb, vectorS, &rcond, &rank, work, &lwork, &info);
    
    /* Check for the full rank */
    if( info > 0 ) {
        printf( "The diagonal element %i of the triangular factor ", (int) info );
        printf( "of A is zero, so that A does not have full rank;\r\n" );
        printf( "the least squares solution could not be computed.\r\n" );
        return 1;
    }
#else
#if defined(_WIN32) || defined(_WIN64)
    LAPACKE_dgelss( LAPACK_COL_MAJOR, n, 6, 1, matrixD, lda, matrixB, ldb, vectorS, rcond, &rank );
#endif
#endif
    
    // check if the condition number is bigger than the allowed maximum
    if( vectorS[0]/vectorS[5] >= MAX_CONDITION_NUMBER) {
        printf( "The condition number (%f) is bigger than the allowed maximum (%d)\r\n ", vectorS[0]/vectorS[5], MAX_CONDITION_NUMBER );
        return 1;
    }
    
    // compute the radii and offsets from the results
    accOffset[0] = (float) -matrixB[3]/matrixB[0];
    accOffset[1] = (float) -matrixB[4]/matrixB[1];
    accOffset[2] = (float) -matrixB[5]/matrixB[2];
    
    gamma = 1 + ( matrixB[3]*matrixB[3] / matrixB[0] + matrixB[4]*matrixB[4] / matrixB[1] + matrixB[5]*matrixB[5] / matrixB[2] );
    accScaling[0] = (float) sqrt(gamma/matrixB[0]);
    accScaling[1] = (float) sqrt(gamma/matrixB[1]);
    accScaling[2] = (float) sqrt(gamma/matrixB[2]);
    
    // compute the condition number
    calData->conditionNumber = vectorS[0]/vectorS[5];
    
    /* Free workspace */
#ifdef __MACH__  // if mach (mac os X)
    free( (void*)work );
#else
#if defined(_WIN32) || defined(_WIN64)
    free(matrixD);
    free(matrixB);
#endif
#endif
    
    return 0;
}