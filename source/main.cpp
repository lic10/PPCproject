// main.cpp, modified from MMSP.main.hpp

// The user must supply the following in any source
// code that includes this file:
//
//     #include"..."
//
//     std::string PROGRAM = "...";
//     std::string MESSAGE = "...";
//     typedef ... GRID2D;
//     typedef ... GRID3D;
//
//     #include"MMSP.main.hpp"
//
//
// The first include must provide the functions
//
//     void generate(int dim,
//                   const char* filename);
//
//     void update(GRID2D& grid, int steps);
//
// which the main() function calls to generate
// example grids or to perform computations.


#ifndef MMSP_MAIN
#define MMSP_MAIN
#include<iostream>
#include<iomanip>
#include<fstream>
#include<sstream>
#include<cstdlib>
#include<cctype>
#ifdef PHASEFIELD
#include"graingrowth.cpp"
#else
#include"graingrowth_MC.cpp"
#endif
#include"rdtsc.h"

template <typename T> int ilength(const T& i)
{
	std::stringstream l;
	l << i;
	return l.str().length();
}

int main(int argc, char* argv[]) {

	MMSP::Init(argc, argv);

	// check argument list
	if (argc < 2) {
		std::cout << PROGRAM << ": bad argument list.  Use\n\n";
		std::cout << "    " << PROGRAM << " --help\n\n";
		std::cout << "to generate help message.\n\n";
		exit(-1);
	}

	int nthreads = 1;
	unsigned int rank=0;
	#ifdef MPI_VERSION
	rank = MPI::COMM_WORLD.Get_rank();
	#endif


	#ifdef BGQ
	double clock_rate=1600000000.0;
	#else
	// Read clock rate from GNU/Linux machine
	double clock_rate=2666700000.0;
	std::ifstream fh("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies");
	if (!fh)
		clock_rate=2667000000.0;
	else {
		fh>>clock_rate;
		fh.close();
	}
	clock_rate*=1000.0;
	#endif

  	// print help message and exit
	if (std::string(argv[1]) == std::string("--help")) {
		std::cout << PROGRAM << ": " << MESSAGE << "\n\n";
		std::cout << "Valid command lines have the form:\n";
		std::cout << "    " << PROGRAM << " ";
		std::cout << "[--help] [--init dimension [outfile]] [--nonstop dimension outfile steps [increment]] [infile [outfile] steps [increment]]\n\n";
		std::cout << "A few examples of using the command line follow.\n\n";
		std::cout << "The command\n";
		std::cout << "    " << PROGRAM << " --help\n";
		std::cout << "generates this help message and exits.  ";
		std::cout << "The \"--init\" option can be used to initialize the grid with a Voronoi tessellation, e.g.\n";
		std::cout << "    " << PROGRAM << " --init 2\n";
		std::cout << "generates the Voronoi tessellation on a grid of dimension 2 and writes it to the \n";
		std::cout << "file named \"voronoi.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " --init 3 voronoi.dat\n";
		std::cout << "generates the Voronoi tessellation on a grid of dimension 3 and writes it to the \n";
		std::cout << "file named \"voronoi.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " polycrystal.dat 1000\n";
		std::cout << "reads the grid contained within \"polycrystal.dat\" and runs a simulation for 1000 time steps.\n";
		std::cout << "The final grid is written to a file named \"polycrystal.1000.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " polycrystal.dat 1000 100\n";
		std::cout << "reads the grid contained within \"polycrystal.dat\" and runs a simulation for 1000 time steps.\n";
		std::cout << "The grid is then written to a file every 100 time steps.\n";
		std::cout << "The resulting files are \nnamed \"polycrystal.0100.dat\", \"polycrystal.0200.dat\", ... \"polycrystal.1000.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " voronoi.dat polycrystal.dat 1000\n";
		std::cout << "reads the grid contained within \"voronoi.dat\" and runs a simulation for 1000 time steps.\n";
		std::cout << "The final grid is written to a file named \"polycrystal.1000.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " polycrystal.0000.dat 1000 100\n";
		std::cout << "reads the grid contained within \"polycrystal.0000.dat\" and runs a simulation for 1000 time steps.\n";
		std::cout << "The grid is then written to a file every 100 time steps.\n";
		std::cout << "The resulting files are \nnamed \"polycrystal.0100.dat\", \"polycrystal.0200.dat\", ... \"polycrystal.1000.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " polycrystal.1000.dat 2000 100\n";
		std::cout << "reads the grid contained within \"polycrystal.1000.dat\" and runs a simulation for 1000 additional time steps.\n";
		std::cout << "The grid is then written to a file every 100 time steps.\n";
		std::cout << "The resulting files are named \n\"polycrystal.1100.dat\", \"polycrystal.1200.dat\", ... \"polycrystal.2000.dat\".\n";
		std::cout << std::endl;
		std::cout << "    " << PROGRAM << " --nonstop 3 polycrystal.0000.dat 1000 100 2\n";
		std::cout << "generates the Voronoi tessellation on a grid of dimension 3 and writes it to the\n";
		std::cout << "file named \"polycrystal.0000.dat\", then runs a simulation for 1000 time steps.\n";
		std::cout << "The grid is then written to a file every 100 time steps.\n";
		std::cout << "The resulting files are named \n\"polycrystal.0100.dat\", \"polycrystal.0200.dat\", ... \"polycrystal.1000.dat\".\n";
		std::cout << "number of pthreads is 2\n";
		std::cout << std::endl;
		exit(0);
	}



	// generate initial grid
	else if (std::string(argv[1]) == std::string("--init")) {
		// check argument list
		if (argc<3 or argc>4) {
			std::cout << PROGRAM << ": bad argument list.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		// check problem dimension
		if (std::string(argv[2]).find_first_not_of("0123456789") != std::string::npos) {
			std::cout << PROGRAM << ": initial grid must have integral dimension.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		int dim = atoi(argv[2]);

		// dimension must be 2 or 3
		if (dim<2 or dim>3) {
			std::cout<<PROGRAM<<": initial grid must be of dimension 2 or 3.  Use\n\n";
			std::cout<<"    "<<PROGRAM<<" --help\n\n";
			std::cout<<"to generate help message.\n\n";
			exit(-1);
		}

		// set output file name
		std::string outfile;
		if (argc < 4) outfile = "voronoi.dat";
		else outfile = argv[3];

		// tessellate
		char filename[FILENAME_MAX] = { }; //new char[outfile.length()+2];
		for (unsigned int i=0; i<outfile.length(); i++)
			filename[i] = outfile[i];
		//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';
		MMSP::generate(dim, filename, 0, nthreads);
	}



	// run tessellation & simulation
	else if (std::string(argv[1]) == std::string("--nonstop")) {
		// bad argument list
		if (argc!=7) {
			std::cout << PROGRAM << ": bad argument list.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		// check problem dimension
		if (std::string(argv[2]).find_first_not_of("0123456789") != std::string::npos) {
			std::cout << PROGRAM << ": initial grid must have integral dimension.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		int dim = atoi(argv[2]);

		// dimension must be 2 or 3
		if (dim<2 or dim>3) {
			std::cout<<PROGRAM<<": initial grid must be of dimension 2 or 3.  Use\n\n";
			std::cout<<"    "<<PROGRAM<<" --help\n\n";
			std::cout<<"to generate help message.\n\n";
			exit(-1);
		}

		// set output file name
		const std::string outfile(argv[3]);

		// must have integral number of time steps
		if (std::string(argv[4]).find_first_not_of("0123456789") != std::string::npos) {
			std::cout << PROGRAM << ": number of time steps must have integral value.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		int steps=atoi(argv[4]);
		// must have integral output increment
		if (std::string(argv[5]).find_first_not_of("0123456789") != std::string::npos) {
			std::cout << PROGRAM << ": output increment must have integral value.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		int increment = atoi(argv[5]);
		// output increment must be smaller than number of steps
		if (increment > steps) {
			std::cout << PROGRAM << ": output increment must be smaller than number of time steps.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		nthreads = atoi(argv[6]);
		// must have integral output increment
		if (std::string(argv[6]).find_first_not_of("0123456789") != std::string::npos) {
			std::cout << PROGRAM << ": nthreads must have integral value.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

    		// set output file basename
		int iterations_start = 0;
		std::string base;
		const int last_dot = outfile.find_last_of(".");
		if (outfile.find_last_of(".")==std::string::npos) // no dot found
			base = outfile + ".";
		else if (outfile.rfind(".", last_dot - 1) == std::string::npos) // only one dot found
			base = outfile.substr(0, last_dot) + ".";
		else {
			int prev_dot = outfile.rfind(".", last_dot - 1);
			std::string number = outfile.substr(prev_dot + 1, last_dot - prev_dot - 1);
			bool isNumeric(true);
			for (unsigned int i = 0; i < number.size() && isNumeric; ++i)
				if (!isdigit(number[i])) isNumeric = false;
			if (isNumeric)
				base = outfile.substr(0, prev_dot) + ".";
			else base = outfile.substr(0, last_dot) + ".";
		}
		#ifdef DEBUG
		if (rank==0) std::cout<<"Filename base is "<<base<<std::endl;
		#endif

		// set output file suffix
		std::string suffix = "";
		if (outfile.find_last_of(".") != std::string::npos)
			suffix = outfile.substr(outfile.find_last_of("."), std::string::npos);
		else suffix = "dat";

		// set output filename length
		int length = base.length() + suffix.length() + ilength(steps);

		if (dim == 2) {
			// tessellate
			unsigned long timer = rdtsc();
			GRID2D* grid=MMSP::generate<2>(0, nthreads);
			#ifndef SILENT
			if (rank==0) std::cout<<"Finished tessellation in "<<(rdtsc() - timer)/clock_rate<<" sec."<<std::endl;
			#endif
			assert(grid!=NULL);
			char filename[FILENAME_MAX] = { }; //new char[outfile.length()+2];
			for (unsigned int i=0; i<outfile.length(); i++)
				filename[i] = outfile[i];
			//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';

			// write initialized grid to file
			unsigned long iotimer = rdtsc();
			double clockbw = 0.0;
			#ifdef BGQ
			clockbw = MMSP::output_bgq(*grid, filename);
			clockbw *= clock_rate;
			#else
			MMSP::output(*grid, filename);
			#endif
			iotimer = rdtsc() - iotimer;
			unsigned long allio=0;
			double allbw = 0.0;
			#ifdef MPI_VERSION
			MPI_Reduce(&iotimer, &allio, 1, MPI_UNSIGNED_LONG, MPI_MAX, 0, MPI::COMM_WORLD);
			MPI_Reduce(&clockbw, &allbw, 1, MPI_DOUBLE, MPI_SUM, 0, MPI::COMM_WORLD);
			#else
			allio=iotimer;
			#endif
			#ifndef SILENT
			if (rank==0) std::cout<<"Wrote "<<outfile<<" in "<<allio/clock_rate<<" sec. MP Write bandwidth was "<<allbw<<" B/s, excluding aggregation overhead."<<std::endl;
			#endif

			// perform computation
			for (int i = iterations_start; i < steps; i += increment) {
				MMSP::update(*grid, increment, nthreads);

				// generate output filename
				std::stringstream outstr;
				outstr << base;
				while (outstr.str().length() < length - ilength(i+increment) - suffix.length())
					outstr << '0';
				outstr << i+increment << suffix;

				// write grid output to file
				char filename[FILENAME_MAX] = { }; //new char[outstr.str().length()+2];
				for (unsigned int i=0; i<outstr.str().length(); i++)
					filename[i] = outstr.str()[i];
				//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';
				iotimer = rdtsc();
				#ifdef DEBUG
				if (rank==0) std::cout<<"Writing "<<std::string(filename)<<std::endl;
				#endif
				//#if defined(BGQ) && defined(PHASEFIELD)
				#ifdef BGQ
				MMSP::output_bgq(*grid, filename);
				#else
				MMSP::output(*grid, filename);
				#endif
				iotimer = rdtsc() - iotimer;
				#ifdef MPI_VERSION
				MPI_Reduce(&iotimer, &allio, 1, MPI_UNSIGNED_LONG, MPI_MAX, 0, MPI::COMM_WORLD);
				#else
				allio = iotimer;
				#endif
				#ifndef SILENT
				if (rank==0) std::cout<<"Wrote "<<outfile<<" in "<<allio/clock_rate<<" sec."<<std::endl;
				#endif
				outstr.str("");
			}
			if (grid!=NULL) delete grid; grid=NULL;
		}

		if (dim == 3) {
			// tessellate
			unsigned long timer = rdtsc();
			GRID3D* grid=MMSP::generate<3>(0, nthreads);
			#ifndef SILENT
			if (rank==0) std::cout<<"Finished tessellation in "<<(rdtsc() - timer)/clock_rate<<" sec."<<std::endl;
			#endif
			assert(grid!=NULL);
			char filename[FILENAME_MAX] = { }; //new char[outfile.length()+2];
			for (unsigned int i=0; i<outfile.length(); i++)
				filename[i] = outfile[i];
			//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';
			unsigned long iotimer = rdtsc();
			//#if defined(BGQ) && defined(PHASEFIELD)
			#ifdef BGQ
			MMSP::output_bgq(*grid, filename);
			#else
			MMSP::output(*grid, filename);
			#endif
			iotimer = rdtsc() - iotimer;
			unsigned long allio = 0;
			#ifdef MPI_VERSION
			MPI_Reduce(&iotimer, &allio, 1, MPI_UNSIGNED_LONG, MPI_MAX, 0, MPI::COMM_WORLD);
			#else
			allio = iotimer;
			#endif
			#ifndef SILENT
			if (rank==0) std::cout<<"Wrote "<<outfile<<" in "<<allio/clock_rate<<" sec."<<std::endl;
			#endif

			// perform computation
			for (int i = iterations_start; i < steps; i += increment) {
				MMSP::update(*grid, increment, nthreads);

				// generate output filename
				std::stringstream outstr;
				outstr << base;
				while (outstr.str().length() < length - ilength(i+increment) - suffix.length())
					outstr << '0';
				outstr << i + increment << suffix;

				// write grid output to file
				char filename[FILENAME_MAX] = { }; //new char[outstr.str().length()+2];
				for (unsigned int i=0; i<outstr.str().length(); i++)
					filename[i] = outstr.str()[i];
				//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';
				iotimer = rdtsc();
				#ifdef DEBUG
				if (rank==0) std::cout<<"Writing "<<std::string(filename)<<std::endl;
				#endif
				//#if defined(BGQ) && defined(PHASEFIELD)
				#ifdef BGQ
				MMSP::output_bgq(*grid, filename);
				#else
				MMSP::output(*grid, filename);
				#endif
				iotimer = rdtsc() - iotimer;
				#ifdef MPI_VERSION
				MPI_Reduce(&iotimer, &allio, 1, MPI_UNSIGNED_LONG, MPI_MAX, 0, MPI::COMM_WORLD);
				#else
				allio = iotimer;
				#endif
				#ifndef SILENT
				if (rank==0) std::cout<<"Wrote "<<outfile<<" in "<<allio/clock_rate<<" sec."<<std::endl;
				#endif
				outstr.str("");
			}
			if (grid!=NULL) delete grid; grid=NULL;
		}

	}



	// run simulation
	else {
		// bad argument list
		if (argc<3 or argc>5) {
			std::cout << PROGRAM << ": bad argument list.  Use\n\n";
			std::cout << "    " << PROGRAM << " --help\n\n";
			std::cout << "to generate help message.\n\n";
			exit(-1);
		}

		int steps;
		int increment;
		std::string outfile;

		if (std::string(argv[2]).find_first_not_of("0123456789") == std::string::npos) {
			// set output file name
			outfile = argv[1];

			// must have integral number of time steps
			if (std::string(argv[2]).find_first_not_of("0123456789") != std::string::npos) {
				std::cout << PROGRAM << ": number of time steps must have integral value.  Use\n\n";
				std::cout << "    " << PROGRAM << " --help\n\n";
				std::cout << "to generate help message.\n\n";
				exit(-1);
			}

			steps = atoi(argv[2]);
			increment = steps;

			if (argc > 3) {
				// must have integral output increment
				if (std::string(argv[3]).find_first_not_of("0123456789") != std::string::npos) {
					std::cout << PROGRAM << ": output increment must have integral value.  Use\n\n";
					std::cout << "    " << PROGRAM << " --help\n\n";
					std::cout << "to generate help message.\n\n";
					exit(-1);
				}

				increment = atoi(argv[3]);

				// output increment must be smaller than number of steps
				if (increment > steps) {
					std::cout << PROGRAM << ": output increment must be smaller than number of time steps.  Use\n\n";
					std::cout << "    " << PROGRAM << " --help\n\n";
					std::cout << "to generate help message.\n\n";
					exit(-1);
				}
			}
		}

		else {
			// set output file name
			outfile = argv[2];

			// set number of time steps
			if (std::string(argv[3]).find_first_not_of("0123456789") != std::string::npos) {
				// must have integral number of time steps
				std::cout << PROGRAM << ": number of time steps must have integral value.  Use\n\n";
				std::cout << "    " << PROGRAM << " --help\n\n";
				std::cout << "to generate help message.\n\n";
				exit(-1);
			}

			steps = atoi(argv[3]);
			increment = steps;

			if (argc > 4) {
				// must have integral output increment
				if (std::string(argv[4]).find_first_not_of("0123456789") != std::string::npos) {
					std::cout << PROGRAM << ": output increment must have integral value.  Use\n\n";
					std::cout << "    " << PROGRAM << " --help\n\n";
					std::cout << "to generate help message.\n\n";
					exit(-1);
				}

				increment = atoi(argv[4]);

				// output increment must be smaller than number of steps
				if (increment > steps) {
					std::cout << PROGRAM << ": output increment must be smaller than number of time steps.  Use\n\n";
					std::cout << "    " << PROGRAM << " --help\n\n";
					std::cout << "to generate help message.\n\n";
					exit(-1);
				}
			}
		}

		// file open error check
		std::ifstream input(argv[1]);
		if (!input) {
			std::cerr << "File input error: could not open " << argv[1] << ".\n\n";
			exit(-1);
		}

		// read data type
		std::string type;
		getline(input, type, '\n');

		// grid type error check
		if (type.substr(0, 4) != "grid") {
			std::cerr << "File input error: file does not contain grid data." << std::endl;
			exit(-1);
		}

		// read grid dimension
		int dim;
		input >> dim;

		// set output file basename
		int iterations_start(0);
		if (outfile.find_first_of(".") != outfile.find_last_of(".")) {
			std::string number = outfile.substr(outfile.find_first_of(".") + 1, outfile.find_last_of(".") - 1);
			iterations_start = atoi(number.c_str());
		}
		std::string base;
		if (outfile.rfind(".", outfile.find_last_of(".") - 1) == std::string::npos) // only one dot found
			base = outfile.substr(0, outfile.find_last_of(".")) + ".";
		else {
			int last_dot = outfile.find_last_of(".");
			int prev_dot = outfile.rfind('.', last_dot - 1);
			std::string number = outfile.substr(prev_dot + 1, last_dot - prev_dot - 1);
			bool isNumeric(true);
			for (unsigned int i = 0; i < number.size(); ++i) {
				if (!isdigit(number[i])) isNumeric = false;
			}
			if (isNumeric)
				base = outfile.substr(0, outfile.rfind(".", outfile.find_last_of(".") - 1)) + ".";
			else base = outfile.substr(0, outfile.find_last_of(".")) + ".";
		}

		// set output file suffix
		std::string suffix = "";
		if (outfile.find_last_of(".") != std::string::npos)
			suffix = outfile.substr(outfile.find_last_of("."), std::string::npos);

		// set output filename length
		int length = base.length() + suffix.length() + ilength(steps);

		if (dim == 2) {
			// construct grid object
			GRID2D grid(argv[1]);

			// perform computation
			for (int i = iterations_start; i < steps; i += increment) {

				MMSP::update(grid, increment, nthreads);

				// generate output filename
				std::stringstream outstr;
				outstr << base;
				while (outstr.str().length() < length - ilength(i+increment) - suffix.length())
					outstr << '0';
				outstr << i + increment << suffix;

				// write grid output to file
				char filename[FILENAME_MAX] = { }; //new char[outstr.str().length()+2];
				for (unsigned int i=0; i<outstr.str().length(); i++)
					filename[i] = outstr.str()[i];
				//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';
				#ifdef DEBUG
				if (rank==0) std::cout<<"Writing "<<std::string(filename)<<std::endl;
				#endif
				//#if defined(BGQ) && defined(PHASEFIELD)
				#ifdef BGQ
				MMSP::output_bgq(grid, filename);
				#else
				MMSP::output(grid, filename);
				#endif
				outstr.str("");
			}
		}

		if (dim == 3) {
			// construct grid object
			GRID3D grid(argv[1]);

			// perform computation
			for (int i = iterations_start; i < steps; i += increment) {
				MMSP::update(grid, increment, nthreads);

				// generate output filename
				std::stringstream outstr;
				outstr << base;
				while (outstr.str().length() < length - ilength(i+increment) - suffix.length())
					outstr << '0';
				outstr << i + increment << suffix;

				// write grid output to file
				char filename[FILENAME_MAX] = { }; //new char[outstr.str().length()+2];
				for (unsigned int i=0; i<outstr.str().length(); i++)
					filename[i] = outstr.str()[i];
				//for (unsigned int i=outfile.length(); i<FILENAME_MAX; i++) filename[i] = '\0';
				#ifdef DEBUG
				if (rank==0) std::cout<<"Writing "<<std::string(filename)<<std::endl;
				#endif
				//#if defined(BGQ) && defined(PHASEFIELD)
				#ifdef BGQ
				MMSP::output_bgq(grid, filename);
				#else
				MMSP::output(grid, filename);
				#endif
				outstr.str("");
			}
		}
	}

	MMSP::Finalize();
}

#endif

// Formatted using astyle:
//  astyle --style=linux --indent-col1-comments --indent=tab --indent-preprocessor --pad-header --align-pointer=type --keep-one-line-blocks --suffix=none

