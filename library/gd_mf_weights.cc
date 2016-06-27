#include <stdio.h>
#include "../vowpalwabbit/parser.h"
#include "../vowpalwabbit/vw.h"
#include <fstream>
#include <iostream>
#include <string.h>
#include <boost/program_options.hpp>

using namespace std;
namespace po = boost::program_options;


int main(int argc, char *argv[])
{ string infile;
  string outdir(".");
  string vwparams;

  po::variables_map vm;
  po::options_description desc("Allowed options");
  desc.add_options()
  ("help,h", "produce help message")
  ("infile,I", po::value<string>(&infile), "input (in vw format) of weights to extract")
  ("outdir,O", po::value<string>(&outdir), "directory to write model files to (default: .)")
  ("vwparams", po::value<string>(&vwparams), "vw parameters for model instantiation (-i model.reg -t ...")
  ;

  try
  { po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  }
  catch(exception & e)
  { cout << endl << argv[0] << ": " << e.what() << endl << endl << desc << endl;
    exit(2);
  }

  if (vm.count("help") || infile.empty() || vwparams.empty())
  { cout << "Dumps weights for matrix factorization model (gd_mf)." << endl;
    cout << "The constant will be written to <outdir>/constant." << endl;
    cout << "Linear and quadratic weights corresponding to the input features will be " << endl;
    cout << "written to <outdir>/<ns>.linear and <outdir>/<ns>.quadratic,respectively." << endl;
    cout << endl;
    cout << desc << "\n";
    cout << "Example usage:" << endl;
    cout << "    Extract weights for user 42 and item 7 under randomly initialized rank 10 model:" << endl;
    cout << "    echo '|u 42 |i 7' | ./gd_mf_weights -I /dev/stdin --vwparams '-q ui --rank 10'" << endl;
    return 1;
  }

  // initialize model
  vw* model = VW::initialize(vwparams);
  model->audit = true;

  string target("--rank ");
  size_t loc = vwparams.find(target);
  const char* location = vwparams.c_str()+loc+target.size();
  size_t rank = atoi(location);

  // global model params
  unsigned char left_ns = model->pairs[0][0];
  unsigned char right_ns = model->pairs[0][1];
  weight_vector weights = model->wv;

  // const char *filename = argv[0];
  FILE* file = fopen(infile.c_str(), "r");
  char* line = NULL;
  size_t len = 0;
  ssize_t read;

  // output files
  ofstream constant((outdir + string("/") + string("constant")).c_str()),
           left_linear((outdir + string("/") + string(1, left_ns) + string(".linear")).c_str()),
           left_quadratic((outdir + string("/") + string(1, left_ns) + string(".quadratic")).c_str()),
           right_linear((outdir + string("/") + string(1, right_ns) + string(".linear")).c_str()),
           right_quadratic((outdir + string("/") + string(1, right_ns) + string(".quadratic")).c_str());

  example *ec = NULL;
  while ((read = getline(&line, &len, file)) != -1)
  { line[strlen(line)-1] = 0; // chop

    ec = VW::read_example(*model, line);

    // write out features for left namespace
    features& left = ec->feature_space[left_ns];
    for (size_t i = 0; i < left.size(); ++i)
      { left_linear << left.space_names[i].get()->second << '\t' << weights[left.indicies[i]];

        left_quadratic << left.space_names[i].get()->second;
        for (size_t k = 1; k <= rank; k++)
          left_quadratic << '\t' << weights[(left.indicies[i] + k)];
      }
    left_linear << endl;
    left_quadratic << endl;

    // write out features for right namespace
    features& right = ec->feature_space[right_ns];
    for (size_t i = 0; i < right.size(); ++i)
      { right_linear << right.space_names[i].get()->second << '\t' << weights[left.indicies[i]];

        right_quadratic << right.space_names[i].get()->second;
        for (size_t k = 1; k <= rank; k++)
          right_quadratic << '\t' << weights[(left.indicies[i] + k + rank)];
      }
    right_linear << endl;
    right_quadratic << endl;

    VW::finish_example(*model, ec);
  }

  // write constant
  constant << weights[ec->feature_space[constant_namespace].indicies[0]] << endl;

  // clean up
  VW::finish(*model);
  fclose(file);
}
