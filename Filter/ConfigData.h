/********************************************************************************/
// Copyright (c) 2013 Carlos Becker                                             //
// Ecole Polytechnique Federale de Lausanne                                     //
// Contact <carlos.becker@epfl.ch> for comments & bug reports                   //
//                                                                              //
// This program is free software: you can redistribute it and/or modify         //
// it under the terms of the version 3 of the GNU General Public License        //
// as published by the Free Software Foundation.                                //
//                                                                              //
// This program is distributed in the hope that it will be useful, but          //
// WITHOUT ANY WARRANTY; without even the implied warranty of                   //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU             //
// General Public License for more details.                                     //
//                                                                              //
// You should have received a copy of the GNU General Public License            //
// along with this program. If not, see <http://www.gnu.org/licenses/>.         //
/********************************************************************************/

#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#include <string>
#include <vector>
#include <iostream>

//FIXME I don't like this, but const string inside a tempalted class is tricky
#define FEATUREEXTENSION ".mha"

template<typename ItkImageType>
struct SetConfigData
{
    typename ItkImageType::Pointer rawVolumeImage;
    typename ItkImageType::Pointer groundTruthImage;

    typename ItkImageType::RegionType annotatedRegion;

    std::string featuresRawVolumeImageHash;

    std::string groundTruth;

    std::string id;

//    std::string cacheDir;

    std::string orientEstimate;

    std::vector< std::string > otherFeatures;
    
    // level of anisotropy of the z direction, for anisotropic stacks
    // 1.0 implies that it is isotropic, and k means that spacing in z is k times the spacing in x or y
    double zAnisotropyFactor;

    void printInfo()
    {
        std::cout << "GT: " << groundTruth << std::endl;
        std::cout << "Orient: " << orientEstimate << std::endl;
        std::cout << "Z anisotropy factor: " << zAnisotropyFactor << std::endl;
        
        for (unsigned i=0; i < otherFeatures.size(); i++)
            std::cout << "Otherfeature[" << i << "]: " << otherFeatures[i] << std::endl;
    }

    static void setDefaultSet(SetConfigData& setCfgData, const std::string id = ""){

        setCfgData.id = id;

        setCfgData.featuresRawVolumeImageHash = "";

        // orientation estimate, take the "-repolarized" output of computeSynapseFeatures.py
        setCfgData.orientEstimate = id + std::string("hessOrient-s3.5-repolarized") + FEATUREEXTENSION;

        setCfgData.otherFeatures.clear();
        // these are the feature channels, which must be precomputed with computeSynapseFeatures.py
        setCfgData.otherFeatures.push_back(id + std::string("gradient-magnitude-s1.0") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("gradient-magnitude-s1.6") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("gradient-magnitude-s3.5") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("gradient-magnitude-s5.0") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("stensor-s0.5-r1.0") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("stensor-s0.8-r1.6") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("stensor-s1.8-r3.5") + FEATUREEXTENSION);
        setCfgData.otherFeatures.push_back(id + std::string("stensor-s2.5-r5.0") + FEATUREEXTENSION);

    }

};

template<typename ItkImageType>
class ConfigData
{
public:
    ConfigData(){
        preset = ConfigData::SYNAPSE;

        TPQuantile = 0.1;
        FPQuantile = 0.01;

        gtNegativeLabel = 128;
        gtPositiveLabel = 255;

        // number of adaboost stumps (recommended >= 1000, but in general 400 does well enough)
        numStumps = 500;

        // output base file name. Output files will have this as the first part of their name
        outFileName = "substack";

        probabilisticOutputFilename = "probabilisticOutputVolume.tif";
        usePreview = true;

        //TODO turn back to not saving
        saveIntermediateVolumes = true;
        forceRecomputeFeatures = false;
        minComponentSize = 250;
        maxNumObjects = 200;

        // Number of regions to split the volume into
        numPredictRegions = 1;

        //supervoxel seed and cubeness
        svoxSeed = 1;//3;
        svoxCubeness = 16;//16;

        rawVolume = "ccboostcache";

        //std::string featuresPath = "/home/monso/code/data/synapse_features/";
        //FIXME architecture dependent--> add portability
        cacheDir = "./";

        automaticComputation = false;

        structuredROIs = true;

    }

    typename ItkImageType::Pointer originalVolumeImage;

    std::vector<SetConfigData<ItkImageType> > train;
    std::vector<SetConfigData<ItkImageType> > test;

    std::vector<SetConfigData<ItkImageType> > pendingTrain;

    //FIXME use scoped enums when we switch to C++11
    enum Preset {
        SYNAPSE,
        MITOCHONDRIA
    };
    Preset preset;

    // used for thresholding, see QuantilePostLearnFunctor class
    float TPQuantile, FPQuantile;

    int gtNegativeLabel;
    int gtPositiveLabel;

    unsigned int numStumps;
    std::string outFileName;

    /** Supervoxel seed
      * @deprecated Supervoxels are not used since they are not useful in this case
     **/
    unsigned int svoxSeed;

    /** Supervoxel cubeness
      * @deprecated Supervoxels are not used since they are not useful in this case
     **/
    unsigned int svoxCubeness;

    /** Save the volumes in each processing step.
     *  Useful for debugging */
    bool saveIntermediateVolumes;

    /** Force the recomputation of the features,
     * regardless of them being on disk.
     * If the algorithm fails, it is recommended
     * to activate it once to ensure a sane state.
     */
    bool forceRecomputeFeatures;

    /** True if we use the preview panel before creating the segmentations,
     * or if it is computed within the ccboost task
     */
    bool usePreview;

    /** Unsupported automatic computation flag */
    bool automaticComputation;

    /** True if the ROIs are ordered
     * and constitues the full volume
     * or a single cropped version,
     * false if the ROIs aren't necessarily contiguous
     */
    bool structuredROIs;

    unsigned int numPredictRegions;

    int minComponentSize;

    int maxNumObjects;

    std::string cacheDir;

    std::string rawVolume;

    std::string probabilisticOutputFilename;

public:
    /* Static call to set a ConfigData to its default values
     *
     */
    void reset(){
        preset = ConfigData::SYNAPSE;

        TPQuantile = 0.1;
        FPQuantile = 0.01;

        gtNegativeLabel = 128;
        gtPositiveLabel = 255;

        // number of adaboost stumps (recommended >= 1000, but in general 400 does well enough)
        numStumps = 500;

        // output base file name. Output files will have this as the first part of their name
        outFileName = "substack";

        //TODO turn back to not saving
        saveIntermediateVolumes = true;
        forceRecomputeFeatures = false;
        usePreview = true;
        minComponentSize = 250;
        maxNumObjects = 200;

        // Number of regions to split the volume into
        numPredictRegions = 1;

        //supervoxel seed and cubeness
        svoxSeed = 1;//3;
        svoxCubeness = 16;//16;

        rawVolume = "ccboostcache";

        probabilisticOutputFilename = "probabilisticOutputVolume.tif";

        //std::string featuresPath = "/home/monso/code/data/synapse_features/";
        //FIXME architecture dependent--> add portability
        cacheDir = "./";

        automaticComputation = false;

        structuredROIs = true;
    }

    void printInfo()
    {

        for (unsigned i=0; i < test.size(); i++)
        {
            std::cout << std::endl << "---- TEST " << i << "-----" << std::endl;
            test[i].printInfo();
        }

        std::cout << "SVox seed: " << svoxSeed <<  std::endl;
        std::cout << "SVox cubeness: " << svoxCubeness <<  std::endl;
        std::cout << "Num stumps: " << numStumps << std::endl;
        std::cout << "Outfilename: " << outFileName << std::endl;
    }

};

#endif // CONFIGDATA_H
