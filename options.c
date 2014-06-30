#include <unistd.h>
#include "all.h"

options_t *options_get(int argc, char *argv[])
{
	int option;
	options_t *o = malloc(sizeof(options_t));
	o->infile = NULL;
	o->outfile = NULL;
	o->start  = -1;
	o->number = -1;
	o->verbose = 0;
	while((option = getopt(argc, argv, "o:s:n:v")) > 0) {
		switch(option) {
			case 'o':
				o->outfile = optarg;
				break;
			case 's':
				o->start = atoi(optarg);
				break;
			case 'n':
				o->number = atoi(optarg);
				break;
			case 'v':
				o->verbose = 1;
				break;
			default:
				printf("usage: %s\n", argv[0]);
				exit(1);
				break;
		}
	}
	if(argc > optind)
		o->infile = argv[optind];
	if(o->verbose) {
		printf("option:  infile %s\n", o->infile);
		printf("option: outfile %s\n", o->outfile);
		printf("option: start   %d\n", o->start);
		printf("option: number  %d\n", o->number);
		printf("option: verbose %d\n", o->verbose);
	}
	return o;
}

