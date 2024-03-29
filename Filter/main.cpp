#include "CcboostAdapter.h"

#include <stdio.h>
#include <argp.h>
#include <sys/stat.h>
#include <ImageSplitter.h>

const char *argp_program_version =
"argex 1.0";

const char *argp_program_bug_address =
"<bug-gnu-utils@gnu.org>";

enum {
    CCBOOST,
    POSTPROCESS
};

/* This structure is used by main to communicate with parse_opt. */
struct arguments
{
//  char *args[2];            /* ARG1 and ARG2 */
  int verbose;              /* The -v flag */
  char *outfile;            /* Argument for -o */
  char *inputVolume, *inputVolumeGT;  /* Arguments for -i and -g */
  int onlypostprocess;                 /* Only apply postprocess */
};

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.
*/
static struct argp_option options[] =
{
    {"verbose", 'v', 0, 0, "Produce verbose output"},
    {"inputvolume", 'i', "STRING1", 0,
     "Input volume to segment"},
    {"groundtruthvolume",   'g', "STRING2", 0,
     "Ground truth with labels: unknown=0, background=128, element=255"},
    {"output", 'o', "OUTFILE", 0,
     "Output Volume file name"},
    {"onlypostprocess",  'p', 0, 0, "Only apply the postprocessing"},
    {0}
};

/*
   PARSER. Field 2 in ARGP.
   Order of parameters: KEY, ARG, STATE.
*/
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments *)state->input;

  switch (key)
    {
    case 'v':
      arguments->verbose = 1;
      break;
    case 'p':
        arguments->onlypostprocess = 1;
        break;
    case 'i':
      arguments->inputVolume = arg;
      break;
    case 'g':
      arguments->inputVolumeGT = arg;
      break;
    case 'o':
      arguments->outfile = arg;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= 2)
    {
      argp_usage(state);
    }
//      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 0)
    {
      argp_usage (state);
    }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/*
   ARGS_DOC. Field 3 in ARGP.
   A description of the non-option command-line arguments
     that we accept.
*/
static char args_doc[] = "InputVolume InputVolumeGT";

/*
  DOC.  Field 4 in ARGP.
  Program documentation.
*/
static char doc[] = "ccboostbin -- A program to call iiboost through command-line\vFrom the CVLab.";

/*
   The ARGP structure itself.
*/
static struct argp argp = {options, parse_opt, args_doc, doc};


void postprocess(const ConfigData<itkVolumeType>& cfgdata,
                 std::string floatInputFilename,
                 itkVolumeType::Pointer& outputSegmentation,
                 std::string outputfilename){

    std::setlocale(LC_NUMERIC, "C");


      qDebug() << "output image";

      typedef itk::ImageFileReader< Matrix3D<float>::ItkImageType > ReaderType;
       ReaderType::Pointer reader = ReaderType::New();
       reader->SetFileName(floatInputFilename);
       reader->Update();

      typedef itk::ImageFileWriter< Matrix3D<float>::ItkImageType > fWriterType;
      fWriterType::Pointer writerf = fWriterType::New();

      if(cfgdata.saveIntermediateVolumes) {
          writerf->SetFileName(cfgdata.cacheDir + "predicted.tif");
          writerf->SetInput(reader->GetOutput());
          writerf->Update();
      }
      qDebug() << "predicted.tif created";

      typedef itk::BinaryThresholdImageFilter <Matrix3D<float>::ItkImageType, itkVolumeType>
              fBinaryThresholdImageFilterType;

      int lowerThreshold = 128;
      int upperThreshold = 255;

      fBinaryThresholdImageFilterType::Pointer thresholdFilter
              = fBinaryThresholdImageFilterType::New();
      thresholdFilter->SetInput(reader->GetOutput());
      thresholdFilter->SetLowerThreshold(0);
      thresholdFilter->SetInsideValue(255);
      thresholdFilter->SetOutsideValue(0);
      thresholdFilter->Update();

      typedef itk::ImageFileWriter< itkVolumeType > WriterType;
      WriterType::Pointer writer = WriterType::New();
      if(cfgdata.saveIntermediateVolumes) {
          writer->SetFileName(cfgdata.cacheDir + "predicted-thresholded.tif");
          writer->SetInput(thresholdFilter->GetOutput());
          writer->Update();
      }

      itkVolumeType::Pointer outSeg = thresholdFilter->GetOutput();

      if(cfgdata.saveIntermediateVolumes && outSeg->VerifyRequestedRegion()){
          writer->SetFileName(cfgdata.cacheDir + "thresh-outputSegmentation.tif");
          writer->SetInput(outSeg);
          writer->Update();
      }

      outputSegmentation = outSeg;
      outputSegmentation->DisconnectPipeline();

      CcboostAdapter::removeborders(cfgdata, outputSegmentation);

      writer->SetFileName(outputfilename);
      writer->SetInput(outputSegmentation);
      writer->Update();

      std::vector<itkVolumeType::Pointer> outSegList;
      CcboostAdapter::splitSegmentations(outputSegmentation, outSegList, false, cfgdata.saveIntermediateVolumes, cfgdata.cacheDir);

}



/*
   The main function.
   Notice how now the only function call needed to process
   all command-line options and arguments nicely
   is argp_parse.
*/
int main (int argc, char **argv)
{
  struct arguments arguments;

  /* Set argument defaults */
  arguments.outfile = "predict.tif";
  arguments.inputVolume = "";
  arguments.inputVolumeGT = "";
  arguments.verbose = 0;
  arguments.onlypostprocess = CCBOOST;

#warning remove this default setting of zAnisotropyFactor and add it as a parameter
double zAnisotropyFactor = 2.71296798698;

  /* Where the magic happens */
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  /* Where do we send output? */

  /* Print argument values */
  fprintf (stdout, "-i = %s\n-g = %s\n-o = %s\n\n",
       arguments.inputVolume, arguments.inputVolumeGT, arguments.outfile);
//  fprintf (stdout, "InputVolume = %s\nInputeVolumeGT = %s\n\n",
//       arguments.args[0],
//       arguments.args[1]);

  ConfigData<itkVolumeType> cfgdata;

  /* If in verbose mode, print song stanza */
  if (arguments.verbose)
    cfgdata.saveIntermediateVolumes = true;

  cfgdata.cacheDir = "./output/";
  cfgdata.rawVolume = "supervoxelcache-";

  //FIXME put mkdir inside the core
  mkdir(cfgdata.cacheDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

  try {
      if(arguments.onlypostprocess) {

          itkVolumeType::Pointer outputVolumeITK;

          postprocess(cfgdata,
                      arguments.inputVolume,
                      outputVolumeITK,
                      arguments.outfile);
      } else {

          typedef itk::ImageFileReader< itkVolumeType > ReaderType;
           ReaderType::Pointer reader = ReaderType::New();
           reader->SetFileName(arguments.inputVolume);
           try {
             reader->Update();
           } catch(itk::ExceptionObject &exp){
               std::cout << exp << std::endl;
               return EXIT_FAILURE;
           }

           itkVolumeType::Pointer inputVolumeITK = reader->GetOutput();
           inputVolumeITK->DisconnectPipeline();

          reader->SetFileName(arguments.inputVolumeGT);
          try {
              reader->Update();
          } catch(itk::ExceptionObject &exp){
              std::cout << exp << std::endl;
              return EXIT_FAILURE;
          }

          itkVolumeType::Pointer inputVolumeGTITK = reader->GetOutput();
          inputVolumeGTITK->DisconnectPipeline();

          cfgdata.originalVolumeImage = inputVolumeITK;

          std::vector<FloatTypeImage::Pointer> probabilisticOutSegs;
          std::vector<itkVolumeType::Pointer> outputSegmentations;
          std::vector<itkVolumeType::Pointer> outSegList;
          itkVolumeType::Pointer outputSegmentation = itkVolumeType::New();
          FloatTypeImage::Pointer probOutputSegmentation = FloatTypeImage::New();

          //volume spliting
          //FIXME decide whether or not to divide the volume depending on the memory available and size of it
          bool divideVolume = true;

          if(!divideVolume) {

              SetConfigData<itkVolumeType> trainData;
              SetConfigData<itkVolumeType>::setDefaultSet(trainData);

              trainData.zAnisotropyFactor = zAnisotropyFactor;
              trainData.groundTruthImage = inputVolumeGTITK;
              trainData.rawVolumeImage = inputVolumeITK;
              cfgdata.train.push_back(trainData);

              SetConfigData<itkVolumeType> testData(trainData);
              SetConfigData<itkVolumeType>::setDefaultSet(testData);
              testData.zAnisotropyFactor = zAnisotropyFactor;
              testData.rawVolumeImage = trainData.rawVolumeImage;
              cfgdata.test.push_back(testData);


              CcboostAdapter::core(cfgdata,
                                   probabilisticOutSegs,
                                   outSegList,
                                   outputSegmentations);

              outputSegmentation = outputSegmentations.at(0);

          } else {

              itkVolumeType::RegionType region = inputVolumeITK->GetLargestPossibleRegion();

              std::cout << region << std::endl;

              typedef itk::ImageSplitter< itkVolumeType > SplitterType;

              cfgdata.numPredictRegions = SplitterType::numRegionsFittingInMemory( region.GetSize(),
                                                                                   CcboostAdapter::FREEMEMORYREQUIREDPROPORTIONPREDICT);


              SplitterType splitter(region, cfgdata.numPredictRegions);

              for(int i=0; i < splitter.getCropRegions().size(); i++){

                  std::string id = QString::number(i).toStdString();

                  SetConfigData<itkVolumeType> trainData;
                  SetConfigData<itkVolumeType>::setDefaultSet(trainData, id);

                  trainData.zAnisotropyFactor = zAnisotropyFactor;
                  trainData.rawVolumeImage = splitter.crop(cfgdata.originalVolumeImage,
                                                           splitter.getCropRegions().at(i));
                  trainData.groundTruthImage = splitter.crop(inputVolumeGTITK,
                                                             splitter.getCropRegions().at(i));

                  cfgdata.train.push_back(trainData);

                  SetConfigData<itkVolumeType> testData(trainData);
                  SetConfigData<itkVolumeType>::setDefaultSet(testData, id);
                  testData.zAnisotropyFactor = zAnisotropyFactor;
                  testData.rawVolumeImage = trainData.rawVolumeImage;
                  testData.orientEstimate = trainData.orientEstimate;

                  cfgdata.test.push_back(testData);
              }

              //
              CcboostAdapter::core(cfgdata,
                                   probabilisticOutSegs,
                                   outSegList,
                                   outputSegmentations);

              //

              std::cout << "Core finished" << std::endl;

              //repaste
              splitter.pasteCroppedRegions(outputSegmentations, outputSegmentation);

              typedef itk::ImageSplitter< FloatTypeImage > FloatSplitterType;

              FloatTypeImage::RegionType fregion(region);

              FloatSplitterType fsplitter(fregion,
                                          cfgdata.numPredictRegions,
                                          FloatTypeImage::OffsetType{30,30,0});

              fsplitter.pasteCroppedRegions(probabilisticOutSegs, probOutputSegmentation);

          }

          typedef itk::ImageFileWriter< FloatTypeImage > fWriterType;
          fWriterType::Pointer fwriter = fWriterType::New();
          fwriter->SetFileName(arguments.outfile);
          fwriter->SetInput(probOutputSegmentation);

          typedef itk::ImageFileWriter< itkVolumeType > WriterType;
          WriterType::Pointer writer = WriterType::New();
          writer->SetFileName(std::string("binary") + arguments.outfile);
          writer->SetInput(outputSegmentation);

          try {

              fwriter->Update();
              writer->Update();

          } catch(itk::ExceptionObject &exp){
              std::cout << "Warning: Error writing probabilistic output. what(): " << exp << std::endl;
              return EXIT_FAILURE;
          } catch(...) {
              std::cout << "Warning: Error writing probabilistic output" << std::endl;
              return EXIT_FAILURE;
          }

      }

  } catch(itk::ExceptionObject &exp){
      std::cerr << exp << std::endl;
      return EXIT_FAILURE;
  }


  return EXIT_SUCCESS;
}
