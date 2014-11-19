# README #
### How do I get set up? ###

* Configuration

use the *task-based* branch

add -std=c++11 flag

* Dependencies

deps directory on iiboost
ITK (probably 4)
VTK (probably 4)
qt4
openmp

*Deployment instructions

the binary is ccbooststandalone and is ran like this:

*Running


```
#!bash

./ccbooststandalone -g groundtruth.tif -i volumeinput.tif -o predictionoutputfilename.tif
```


i.e.


```
#!bash

./ccbooststandalone -g Madrid_Train_espinagt_many.tif -i Madrid_Train.tif -o predict.tif
```
