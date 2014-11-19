# README #
### How do I get set up? ###

* Configuration

use the *task-based* branch

add -std=c++11 flag

* Dependencies

deps directory on iiboost
ITK4
qt4
openmp
vtk4
xlslib

i.e.
VTK_DIR                          /home/monso/code/espina-project2/VTK-6.1.0/build                                                                                                                                                                   
 XLSLIB_INCLUDE_DIR               /home/monso/code/espina-project2/xlslib/src                                                                                                                                                                        
 XLSLIB_LIBRARY                   /home/monso/code/espina-project2/xlslib/src/.libs/libxls.so 

*Deployment instructions

the binary is ccbooststandalone and is ran like this:

*Running
./ccbooststandalone -g groundtruth.tif -i volumeinput.tif -o predictionoutputfilename.tif
i.e.
./ccbooststandalone -g Madrid_Train_espinagt_many.tif -i Madrid_Train.tif -o predict.tif