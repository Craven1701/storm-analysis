import os
os.chdir("/Users/ncc-1701-enterprise/Documents/MERFISH_analysis/Data/storm_analysis/jy_testing/")
print(os.getcwd())

import sys
sys.path.append('/Users/ncc-1701-enterprise/Documents/MERFISH_analysis/storm-analysis/') #添加当前路径进入PATH变量以实现模块加载

import storm_analysis.sa_library.parameters as params
import storm_analysis.jupyter_examples.overlay_image as overlay_image
import storm_analysis.daostorm_3d.mufit_analysis as mfit

daop = params.ParametersDAO().initFromFile("example.xml")

# For this data-set, no localizations will be found if threshold is above 25.0
daop.changeAttr("threshold", 8)

daop.changeAttr("find_max_radius", 5) # original value is 5 (pixels)
daop.changeAttr("roi_size", 9) # original value is 9 (pixels)
daop.changeAttr("sigma", 1.5) # original value is 1.5 (pixels)

# Save the changed parameters.
#
# Using pretty = True will create a more human readable XML file. The default value is False.
#

# For this data-set, no localizations will be found if threshold is above 25.0
daop.changeAttr("threshold", 3)

daop.changeAttr("find_max_radius", 5) # original value is 5 (pixels)
daop.changeAttr("roi_size", 15) # original value is 9 (pixels)
daop.changeAttr("sigma", 1.5) # original value is 1.5 (pixels)

# Save the changed parameters.
#
# Using pretty = True will create a more human readable XML file. The default value is False.
#
daop.toXMLFile("testing.xml", pretty = True)
daop.toXMLFile("testing.xml", pretty = True)

if os.path.exists("testing.hdf5"):
    os.remove("testing.hdf5")

mfit.analyze("rawR2.tiff","testing.hdf5", "testing.xml")
overlay_image.overlayImage("rawR2.tiff", "testing.hdf5", 0)
print("Pasue")


    