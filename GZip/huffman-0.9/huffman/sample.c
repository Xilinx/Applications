/***************************************************************************
*                       Huffman Library Usage Sample
*
*   File    : sample.c
*   Purpose : Demonstrates the usage of Huffman library encoding and
*             decoding routines.
*   Author  : Michael Dipperstein
*   Date    : February 25, 2004
*
****************************************************************************
*
* sample: An ANSI C Huffman Encoding/Decoding Library Examples
* Copyright (C) 2004, 2007, 2014 by
* Michael Dipperstein (mdipper@alumni.engr.ucsb.edu)
*
* This file is part of the Huffman library.
*
* The Huffman library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* The Huffman library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "huffman.h"
#include "optlist.h"

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/

typedef enum
{
    SHOW_TREE,
    COMPRESS,
    DECOMPRESS
} mode_t;

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/
static void ShowUsage(FILE *stream, char *progPath);

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : Main
*   Description: This is the main function for this program, it validates
*                the command line input and, if valid, it will build a
*                huffman tree for the input file, huffman encode a file, or
*                decode a huffman encoded file.
*   Parameters : argc - number of parameters
*                argv - parameter list
*   Effects    : Creates specified file from specified inputs
*   Returned   : 0 for success, otherwise errno.
****************************************************************************/
int main (int argc, char *argv[])
{
    int status, canonical;
    option_t *optList, *thisOpt;
    FILE *inFile, *outFile;
    mode_t mode;

    /* initialize variables */
    inFile = NULL;
    outFile = NULL;
    mode = SHOW_TREE;
    canonical = 0;

    /* parse command line */
    optList = GetOptList(argc, argv, "Ccdtni:o:h?");
    thisOpt = optList;

    while (thisOpt != NULL)
    {
        switch(thisOpt->option)
        {
            case 'C':       /* use canonical code */
                canonical = 1;
                break;

            case 'c':       /* compression mode */
                mode = COMPRESS;
                break;

            case 'd':       /* decompression mode */
                mode = DECOMPRESS;
                break;

            case 't':       /* just display tree */
                mode = SHOW_TREE;
                break;

            case 'i':       /* input file name */
                if (inFile != NULL)
                {
                    fprintf(stderr, "Multiple input files not allowed.\n");
                    fclose(inFile);

                    if (outFile != NULL)
                    {
                        fclose(outFile);
                    }

                    FreeOptList(optList);
                    return EINVAL;
                }
                else if ((inFile = fopen(thisOpt->argument, "rb")) == NULL)
                {
                    perror("Opening Input File");

                    if (outFile != NULL)
                    {
                        fclose(outFile);
                    }

                    FreeOptList(optList);
                    return errno;
                }
                break;

            case 'o':       /* output file name */
                if (outFile != NULL)
                {
                    fprintf(stderr, "Multiple output files not allowed.\n");
                    fclose(outFile);

                    if (inFile != NULL)
                    {
                        fclose(inFile);
                    }

                    FreeOptList(optList);
                    return EINVAL;
                }
                else if ((outFile = fopen(thisOpt->argument, "wb")) == NULL)
                {
                    perror("Opening Output File");

                    if (inFile != NULL)
                    {
                        fclose(inFile);
                    }

                    FreeOptList(optList);
                    return errno;
                }
                break;

            case 'h':
            case '?':
                ShowUsage(stdout, argv[0]);
                FreeOptList(optList);
                return 0;
        }

        optList = thisOpt->next;
        free(thisOpt);
        thisOpt = optList;
    }

    /* validate command line */
    if ((inFile == NULL) || (outFile == NULL))
    {
        fprintf(stderr, "Input and output files must be provided\n\n");
        ShowUsage(stderr, argv[0]);
        return EINVAL;
    }

    /* execute selected function */
    switch (mode)
    {
        case SHOW_TREE:
            if (canonical)
            {
                status = CanonicalShowTree(inFile, outFile);
            }
            else
            {
                status = HuffmanShowTree(inFile, outFile);
            }
            break;

        case COMPRESS:
            if (canonical)
            {
                status = CanonicalEncodeFile(inFile, outFile);
            }
            else
            {
                status = HuffmanEncodeFile(inFile, outFile);
            }
            break;

        case DECOMPRESS:
            if (canonical)
            {
                status = CanonicalDecodeFile(inFile, outFile);
            }
            else
            {
                status = HuffmanDecodeFile(inFile, outFile);
            }
            break;

        default:        /* error case */
            status = 0;
            break;
    }

    /* clean up*/
    fclose(inFile);
    fclose(outFile);

    if (0 == status)
    {
        return 0;
    }
    else
    {
        perror("");
        return errno;
    }
}

/****************************************************************************
*   Function   : ShowUsage
*   Description: This function sends instructions for using this program to
*                stdout.
*   Parameters : progPath - the name + path to the executable version of this
*                           program.
*                stream - output stream receiving instructions.
*   Effects    : Usage instructions are sent to stream.
*   Returned   : None
****************************************************************************/
static void ShowUsage(FILE *stream, char *progPath)
{
    fprintf(stream, "Usage: %s <options>\n\n", FindFileName(progPath));
    fprintf(stream, "options:\n");
    fprintf(stream, "  -C : Encode/Decode using a canonical code.\n");
    fprintf(stream, "  -c : Encode input file to output file.\n");
    fprintf(stream, "  -d : Decode input file to output file.\n");
    fprintf(stream,
        "  -t : Generate code tree for input file to output file.\n");
    fprintf(stream, "  -i<filename> : Name of input file.\n");
    fprintf(stream, "  -o<filename> : Name of output file.\n");
    fprintf(stream, "  -h|?  : Print out command line options.\n\n");
}
