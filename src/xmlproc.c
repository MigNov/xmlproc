#include "xml.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Syntax: %s <filename>\n", argv[0]);
		return EXIT_FAILURE;
	}

	processXml(argv[1]);
	return 0;
}

