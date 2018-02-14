#!/usr/bin/env python
"""
Test SMLM movie IO.
"""
import numpy

import storm_analysis

import storm_analysis.sa_library.datareader as datareader
import storm_analysis.sa_library.datawriter as datawriter


def test_io_1():
    """
    Test DAX movie IO.
    """
    movie_h = 50
    movie_w = 40
    movie_l = 10
    
    data = numpy.random.randint(0, 60000, (movie_h, movie_w)).astype(numpy.uint16)

    movie_name = storm_analysis.getPathOutputTest("test_dataio.dax")

    # Write dax movie.
    wr = datawriter.inferWriter(movie_name)
    for i in range(movie_l):
        wr.addFrame(data)
    wr.close()
        
    # Read & check.
    rd = datareader.inferReader(movie_name)
    [mw, mh, ml] = rd.filmSize()
    
    assert(mh == movie_h)
    assert(mw == movie_w)
    assert(ml == movie_l)
    assert(numpy.allclose(data, rd.loadAFrame(0)))

    rd.close()

def test_io_2():
    """
    Test TIF movie IO.
    """
    movie_h = 50
    movie_w = 40
    movie_l = 10
    
    data = numpy.random.randint(0, 60000, (movie_h, movie_w)).astype(numpy.uint16)

    movie_name = storm_analysis.getPathOutputTest("test_dataio.tif")

    # Write tif movie.
    wr = datawriter.inferWriter(movie_name)
    for i in range(movie_l):
        wr.addFrame(data)
    wr.close()
        
    # Read & check.
    rd = datareader.inferReader(movie_name)
    [mw, mh, ml] = rd.filmSize()

    assert(mh == movie_h)
    assert(mw == movie_w)
    assert(ml == movie_l)
    assert(numpy.allclose(data, rd.loadAFrame(0)))

def test_io_3():
    """
    Test FITS movie IO.
    """
    movie_h = 50
    movie_w = 40
    movie_l = 10
    
    data = numpy.random.randint(0, 60000, (movie_h, movie_w)).astype(numpy.uint16)

    movie_name = storm_analysis.getPathOutputTest("test_dataio.fits")

    # Write tif movie.
    wr = datawriter.inferWriter(movie_name)
    for i in range(movie_l):
        wr.addFrame(data)
    wr.close()
        
    # Read & check.
    rd = datareader.inferReader(movie_name)
    [mw, mh, ml] = rd.filmSize()

    assert(mh == movie_h)
    assert(mw == movie_w)
    assert(ml == movie_l)
    assert(numpy.allclose(data, rd.loadAFrame(0)))
    

if (__name__ == "__main__"):
    test_io_1()
    test_io_2()
    test_io_3()
    
