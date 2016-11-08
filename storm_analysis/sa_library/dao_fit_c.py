#!/usr/bin/python
#
# Python interface to the dao_fit C library.
#
# Hazen
#

import ctypes
import math
import numpy
from numpy.ctypeslib import ndpointer
import os
import sys

import storm_analysis.sa_library.ia_utilities_c as utilC
import storm_analysis.sa_library.loadclib as loadclib

daofit = loadclib.loadCLibrary("storm_analysis.sa_library", "_dao_fit")


# C interface definition
daofit.cleanup.argtypes = [ctypes.c_void_p]

daofit.getResidual.argtypes = [ctypes.c_void_p,
                               ndpointer(dtype=numpy.float64)]

daofit.getResults.argtypes = [ctypes.c_void_p,
                              ndpointer(dtype=numpy.float64)]

daofit.getUnconverged.argtypes = [ctypes.c_void_p]
daofit.getUnconverged.restype = ctypes.c_int

daofit.initialize.argtypes = [ndpointer(dtype=numpy.float64),
                              ndpointer(dtype=numpy.float64),
                              ctypes.c_double,
                              ctypes.c_int,
                              ctypes.c_int]
daofit.initialize.restype = ctypes.c_void_p

daofit.initializeZParameters.argtypes = [ctypes.c_void_p,
                                         ndpointer(dtype=numpy.float64), 
                                         ndpointer(dtype=numpy.float64),
                                         ctypes.c_double,
                                         ctypes.c_double]

daofit.iterate2DFixed.argtypes = [ctypes.c_void_p]

daofit.iterate2D.argtypes = [ctypes.c_void_p]

daofit.iterate3D.argtypes = [ctypes.c_void_p]

daofit.iterateZ.argtypes = [ctypes.c_void_p]

daofit.newImage.argtypes = [ctypes.c_void_p,
                            ndpointer(dtype=numpy.float64)]

daofit.newPeaks.argtypes = [ctypes.c_void_p,
                            ndpointer(dtype=numpy.float64),
                            ctypes.c_int]


class MultiFitterException(Exception):
    
    def __init__(self, message):
        Exception.__init__(self, message)


class MultiFitter(object):
    """
    This is designed to be used as follows:

    (1) At the start of the analysis, create a single instance of the appropriate fitting sub-class.
    (2) For each new image, call newImage() once.
    (3) For each iteration of peak fittings, call doFit() with peaks that you want to fit to the image.
    (4) After calling doFit() you can remove peaks that did not fit well with getGoodPeaks().
    (5) After calling doFit() you can use getResidual() to get the current image minus the fit peaks.
    (6) Call cleanup() when you are done with this object and plan to throw it away.

    As all the static variables have been removed from the C library you should 
    be able to use several of these objects simultaneuosly for fitting.

    All of the parameters are optional, use None if they are not relevant.
    """
    def __init__(self, scmos_cal, wx_params, wy_params, min_z, max_z, verbose = False):

        self.default_tol = 1.0e-6
        self.max_z = max_z
        self.min_z = min_z
        self.scmos_cal = scmos_cal
        self.verbose = verbose
        self.wx_params = wx_params
        self.wy_params = wy_params

        self.mfit = None

        # Default clamp parameters.
        #
        # These set the (initial) scale for how much these parameters
        # can change in a single fitting iteration.
        #
        self.clamp = numpy.array([1000.0,   # Height
                                  1.0,      # x position
                                  0.3,      # width in x
                                  1.0,      # y position
                                  0.3,      # width in y
                                  100.0,    # background
                                  0.1])     # z position

    def cleanup(self):
        if self.mfit is not None:
            daofit.cleanup(self.mfit)
        self.mfit = None

    def doFit(self, peaks, max_iterations = 200):

        # Initialize C library with new peaks.
        daofit.newPeaks(self.mfit,
                        numpy.ascontiguousarray(peaks),
                        peaks.shape[0])

        # Iterate fittings.
        i = 0
        self.iterate()
        while(daofit.getUnconverged(self.mfit) and (i < max_iterations)):
            if self.verbose and ((i%20)==0):
                print("iteration", i)
            self.iterate()
            i += 1

        if self.verbose:
            if (i == max_iterations):
                print(" Failed to converge in:", i, daofit.getUnconverged(self.mfit))
            else:
                print(" Multi-fit converged in:", i, daofit.getUnconverged(self.mfit))
            print("")

        # Get updated peak values back from the C library.
        fit_peaks = numpy.ascontiguousarray(numpy.zeros(peaks.shape))
        daofit.getResults(self.mfit, fit_peaks)
        return fit_peaks

    def getGoodPeaks(self, peaks, min_height, min_width):
        """
        Create a new list from peaks containing only those peaks that meet 
        the specified criteria for minimum peak height and width.
        """
        if(peaks.shape[0]>0):
            min_width = 0.5 * min_width

            status_index = utilC.getStatusIndex()
            height_index = utilC.getHeightIndex()
            xwidth_index = utilC.getXWidthIndex()
            ywidth_index = utilC.getYWidthIndex()

            if self.verbose:
                tmp = numpy.ones(peaks.shape[0])                
                print("getGoodPeaks")
                for i in range(peaks.shape[0]):
                    print(i, peaks[i,0], peaks[i,1], peaks[i,3], peaks[i,2], peaks[i,4])
                print("Total peaks:", numpy.sum(tmp))
                print("  fit error:", numpy.sum(tmp[(peaks[:,status_index] != 2.0)]))
                print("  min height:", numpy.sum(tmp[(peaks[:,height_index] > min_height)]))
                print("  min width:", numpy.sum(tmp[(peaks[:,xwidth_index] > min_width) & (peaks[:,ywidth_index] > min_width)]))
                print("")
            mask = (peaks[:,7] != status_index) & (peaks[:,height_index] > min_height) & (peaks[:,xwidth_index] > min_width) & (peaks[:,ywidth_index] > min_width)
            return peaks[mask,:]
        else:
            return peaks

    def getResidual(self):
        residual = numpy.ascontiguousarray(numpy.zeros(self.scmos_cal.shape))
        daofit.getResidual(self.mfit, residual)
        return residual
    
    def iterate(self):
        """
        Sub classes override this to use the correct fitting function.
        """
        raise Exception("iterate() method not defined.")

    def initMFit(self, image):
        """
        This initializes the C fitting library. You can call this directly, but
        the idea is that it will get called automatically the first time that you
        provide a new image for fitting.
        """
        if self.scmos_cal is None:
            self.scmos_cal = numpy.ascontiguousarray(numpy.zeros(image.shape))
        else:
            self.scmos_cal = numpy.ascontiguousarray(self.scmos_cal)

        if (image.shape[0] != self.scmos_cal.shape[0]) or (image.shape[1] != self.scmos_cal.shape[1]):
            raise MultiFitterException("Image shape and sCMOS calibration shape do not match.")

        self.mfit = daofit.initialize(self.scmos_cal,
                                      numpy.ascontiguousarray(self.clamp),
                                      self.default_tol,
                                      self.scmos_cal.shape[1],
                                      self.scmos_cal.shape[0])

        if self.wx_params is not None:
            daofit.initializeZParameters(self.mfit,
                                         numpy.ascontiguousarray(self.wx_params),
                                         numpy.ascontiguousarray(self.wy_params),
                                         self.min_z,
                                         self.max_z)
            
    def newImage(self, image):

        if self.mfit is None:
            self.initMFit(image)

        daofit.newImage(self.mfit, image)


class MultiFitter2DFixed(MultiFitter):
    """
    Fit with a fixed peak width.
    """
    def iterate(self):
        daofit.iterate2DFixed(self.mfit)


class MultiFitter2D(MultiFitter):
    """
    Fit with a variable peak width (of the same size in X and Y).
    """
    def iterate(self):
        daofit.iterate2D(self.mfit)

        
class MultiFitter3D(MultiFitter):
    """
    Fit with peak width that can change independently in X and Y.
    """
    def iterate(self):
        daofit.iterate3D(self.mfit)

        
class MultiFitterZ(MultiFitter):
    """
    Fit with peak width that varies in X and Y as a function of Z.
    """
    def iterate(self):
        daofit.iterateZ(self.mfit)


#
# Other functions.
#
def calcSxSy(wx_params, wy_params, z):
    """
    Return sigma x and sigma y given the z calibration parameters.
    """
    zx = (z - wx_params[1])/wx_params[2]
    sx = 0.5 * wx_params[0] * math.sqrt(1.0 + zx*zx + wx_params[3]*zx*zx*zx + wx_params[4]*zx*zx*zx*zx)
    zy = (z - wy_params[1])/wy_params[2]
    sy = 0.5 * wy_params[0] * math.sqrt(1.0 + zy*zy + wy_params[3]*zy*zy*zy + wy_params[4]*zy*zy*zy*zy)
    return [sx, sy]


#
# The MIT License
#
# Copyright (c) 2016 Zhuang Lab, Harvard University
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#