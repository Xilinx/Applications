#include "xlzma.h"
#include <stdio.h>   // For printf()
#include <string.h>  // For memcmp()
#include <stdlib.h>  // For exit()

/*
 * Easy show-error-and-bail function.
 */
void run_screaming(const char* message, const int code) {
  printf("%s \n", message);
  exit(code);
}


/*
 * main
 */
int main(int argc, char *argv[]) {
  char lzmaFilename[256] = { 0 };
  printf("UNC READING:%s\n",argv[1]);
  FILE *fptr = fopen(argv[1], "r");
  if (fptr == NULL){
        printf("Cannot open file \n");
        exit(0);
  }
  fseek(fptr, 0L, SEEK_END);
  long numbytes = ftell(fptr);
  char *src = (char*)calloc(numbytes, sizeof(char));	
  if(src == NULL)
    return 1;
  rewind (fptr); 
  fread(src, sizeof(char), numbytes, fptr);
  fclose(fptr);
  
  const int src_size = numbytes;
  const int max_dst_size = xlzma_bound(src_size);
  printf("max_dst_size:%d\n",max_dst_size);
  // We will use that size for our destination boundary when allocating space.
  char* compressed_data = (char*) malloc(max_dst_size);
  if (compressed_data == NULL)
    run_screaming("Failed to allocate memory for *compressed_data.", 1);
  
  int handle = xlzma_init();
  int compressed_data_size = xlzma_compress(handle,src,src_size, compressed_data ,max_dst_size);
  // Check return_value to determine what happened.
  if (compressed_data_size < 0) {
    xlzma_close(handle);
    run_screaming("A negative result from xlzma_compress indicates a failure trying to compress the data.", compressed_data_size);
  }
  if (compressed_data_size == 0) {
    xlzma_close(handle);
    run_screaming("A result of 0 means compression worked, but was stopped because the destination buffer couldn't hold all the information.", 1);
  }
  if (compressed_data_size > 0)
    printf("We successfully compressed some data:%u!\n",compressed_data_size);
  // Not only does a positive return_value mean success, the value returned == the number of bytes required.
  // You can use this to realloc() *compress_data to free up memory, if desired.  We'll do so just to demonstrate the concept.

  snprintf(lzmaFilename, 256, "%s.xz", argv[1]);
  printf("C WRITING:%s\n",lzmaFilename);
  FILE* outFp = fopen(lzmaFilename, "wb");
  fwrite(compressed_data, 1, compressed_data_size,outFp);
  fclose(outFp);
  free(compressed_data);
  xlzma_close(handle);
  return 0;
}
