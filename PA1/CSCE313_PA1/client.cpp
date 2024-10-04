/*
	Author of the starter code
	Yifan Ren
	Department of Computer Science & Engineering
	Texas A&M University
	Date: 9/15/2024

	Please include your Name, UIN, and the date below
	Name: Evan Chan
	UIN: 932008696
	Date: 9/27/2024
*/
#include "common.h"
//#include "stdlib.h"
#include "FIFORequestChannel.h"
#include <chrono>

using namespace std;

int main(int argc, char *argv[])
{
	int opt;

	// Can add boolean flag variables if desired
	int p = -1;			 // patient number
	double t = -1;		 // time
	int e = -1;			 // ecg data type 1/2
	int m = MAX_MESSAGE; // message
	string filename = "";
	vector<FIFORequestChannel *> channels;
	// Add other arguments here   |   Need to add -c, -m flags. BE CAREFUL OF getopt() NOTATION

	bool cflag = false;

	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1)
	{
		switch (opt)
		{
		case 'p':
			p = atoi(optarg);
			break;
		case 't':
			t = atof(optarg);
			break;
		case 'e':
			e = atoi(optarg);
			break;
		case 'f':
			filename = optarg;
			// fflag = true;
			break;
		case 'm':
			m = atoi(optarg);
			break;
		case 'c':
			cflag = true;
			break;
		}
	}

	// fork
	// in the child, run execvp using the server

	// Task 1:
	// Run the server process as a child of the client process
	// Fork child process
	pid_t pid1 = fork();
	if (pid1 < 0)
	{
		EXITONERROR("Fork failed");
		return 1;
	}

	

	// Run server
	// SECOND CONDITIONAL NOT EVALUATED BY PARENT
	if (pid1 < 0)
	{
		EXITONERROR("child server process failed");
	}
	else if (pid1 == 0)
	{
		execl("./server", "./server", "-m", (to_string(m).c_str()), nullptr);
		perror("exec failed");
		return 1;
	}
	// Set up FIFORequestChannel
	FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&chan);

	// Task 4:
	// Request a new channel (e.g. -c)
	// Should use this new channel to communicate with server (still use control channel for sending QUIT_MSG)
	if (cflag)
	{
		MESSAGE_TYPE msg = NEWCHANNEL_MSG;
		// Write new channel message into pipe
		chan.cwrite(&msg, sizeof(MESSAGE_TYPE));
		// Read response from pipe (Can create any static sized char array that fits server response, e.g. MAX_MESSAGE)
		char *newPipeName = new char[m];
		chan.cread(newPipeName, sizeof(newPipeName));
		// Create a new FIFORequestChannel object using the name sent by server
		FIFORequestChannel* new_chan = new FIFORequestChannel(newPipeName, FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(new_chan);
	}

	chan = *(channels.back());

	// Task 2.1 + 2.2:
	// Request data points
	if (p != -1 && e != -1 && filename == "")
	{
		char buf[MAX_MESSAGE];
		datamsg x(p, t, e); // Request patient data point

		memcpy(buf, &x, sizeof(datamsg)); // Can either copy datamsg into separate buffer then write buffer into pipe,
		chan.cwrite(&x, sizeof(datamsg)); // or just directly write datamsg into pipe
		double reply;
		chan.cread(&reply, sizeof(double));

		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}
	else if (p != -1 && e == -1 && filename == "")
	{
		// Open x1.csv file under ./received/
		// Can use any method for opening a file, ofstream is one method
		std::ofstream ofs;
		ofs.open("received/x1.csv");

		if (!ofs.is_open())
		{
			std::cerr << "Error opening file!" << std::endl;
			return 1;
		}
		t = 0.0;
		// Iterate 1000 times
		for (int i = 1; i <= 1000; i++)
		{
			// Write time into x1.csv (Time is 0.004 second deviations) TODO

			datamsg msg = datamsg(p, t, 1);
			// Write ecg1 datamsg into pipe
			char buffer[MAX_MESSAGE];
			memcpy(buffer, &msg, sizeof(datamsg));
			chan.cwrite(buffer, sizeof(datamsg));
			// Read response for ecg1 from pipe
			double read1;
			chan.cread(&read1, sizeof(double));

			msg = datamsg(p, t, 2);
			// Request ecg2 TODO
			memcpy(buffer, &msg, sizeof(datamsg));
			chan.cwrite(buffer, sizeof(datamsg));
			// Read response for ecg1 from pipe
			double read2;
			chan.cread(&read2, sizeof(double));

			ofs << t << ',' << read1 << ',' << read2 << endl;
			// Increment time
			t += 0.004;
		}

		// CLOSE YOUR FILE TODO
		ofs.close();
	}
	// Task 3:
	// Request files (e.g. './client -f 1.csv')
	if (filename != "")
	{
		auto start_time = chrono::high_resolution_clock::now();
		filemsg fm(0, 0);		 // Request file length message
		string fname = filename; // Make sure to read filename from command line arguments (given to -f)

		// Calculate file length request message and set up buffer
		int len = sizeof(filemsg) + (fname.size() + 1);
		char *buf2 = new char[len];

		// Copy filemsg fm into msgBuffer, attach filename to the end of filemsg fm in msgBuffer, then write msgBuffer into pipe
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);

		// Read file length response from server for specified file
		__int64_t file_length;
		chan.cread(&file_length, sizeof(__int64_t));
		cout << "The length of " << fname << " is " << file_length << endl;

		// Set up output file under received folder TODO
		// Can use any file opening method
		ofstream ofs;
		ofs.open("received/" + fname);

		// if (!ofs.is_open())
		// {
		// 	std::cerr << "Error opening file!" << std::endl;
		// 	return 1;
		// }

		// Request data chunks from server and output into file
		// Loop from start of file to file_length
		filemsg *freq = (filemsg *)buf2;
		char *buf3 = new char[m];
		for (int64_t i = 0; i < file_length; i += m)
		{
			// Create filemsg for data chunk range
			// Assign data chunk range properly so that the data chunk to fetch from the file does NOT exceed the file length (i.e. take minimum between the two)
			freq->offset = i;
			freq->length = min((int64_t)m, file_length - i);
			// Copy filemsg into buf2 buffer and write into pipe
			// File name need not be re-copied into buf2, as filemsg struct object is staticly sized and therefore the file name is unchanged when filemsg is re-copied into buf2
			chan.cwrite(buf2, len);
			// Read data chunk response from server into separate data buffer
			int read = chan.cread(buf3, freq->length);
			// Write data chunk into new file
			ofs.write(buf3, read);
		}

		// CLOSE YOUR FILE
		ofs.close();
		delete[] buf2;
		delete[] buf3;

		auto end_time = chrono::high_resolution_clock::now();
		// Calculate the duration in milliseconds
		auto duration = chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
		cout << "Transfer Time: " << duration << " ms" << endl;
	}

	// Task 5:
	//  Closing all the channels
	if (cflag)
	{
		while (!channels.empty())
		{
			FIFORequestChannel *temp = channels.back();
			delete temp;
			channels.pop_back();
		}
	}
	MESSAGE_TYPE msg = QUIT_MSG;
	chan.cwrite(&msg, sizeof(MESSAGE_TYPE));
}