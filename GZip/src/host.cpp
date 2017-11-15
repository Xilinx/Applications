#include "lz77.h"
#include <fstream>
#include <vector>
#include "cmdlineparser.h"

uint32_t lz77_encode_top(const char *inFile_name, xil_lz77& lz77) 
{
    FILE *inFile  = NULL;
    FILE *outFile = NULL;
    
    string outFile_name = inFile_name;
    outFile_name = outFile_name + "encode.lz77";
    
    // Open Input and Output (.lz77) file descriptors
    inFile  = fopen(inFile_name, "rb");
    outFile = fopen(outFile_name.c_str(), "wb");
    
    // Error checking
    assert(inFile && "Encode inFile fails to open");
    assert(outFile && "Encode outFile fails to open");
    
    // Find file size
    fseek(inFile, 0, SEEK_END);
    long input_size = ftell(inFile);
    rewind(inFile);
    
    vector<uint8_t,aligned_allocator<uint8_t>> lz77_in (input_size);
    vector<uint8_t,aligned_allocator<uint8_t>> lz77_out(input_size * 2);
    
    uint8_t c = 0;
    
    // Read from input file
    for(int i = 0; i < input_size; i++){
        c = getc(inFile);
        lz77_in[i] = c;
    }

    #ifdef VERBOSE
    printf("LZ77 Encode    -- ");
    #endif

    // LZ77 encoding 
    uint32_t enbytes = lz77.encode(lz77_in.data(), lz77_out.data(), input_size);
   
    for(uint32_t i = 0; i < enbytes; i++)
        putc(lz77_out[i], outFile); 

    // Close file 
    fclose(inFile);
    fclose(outFile); 
    return enbytes;
}

int lz77_decode_top(const std::string& inFile_name,uint32_t enbytes) {
    
    FILE *inFile  = NULL;
    inFile  = fopen(inFile_name.c_str(), "rb");
    
    // Find file size
    fseek(inFile, 0, SEEK_END);
    long input_size = ftell(inFile);
    rewind(inFile);
    
    uint8_t *decode_in  = (uint8_t *) malloc(sizeof(uint8_t) * enbytes);
    uint8_t *decode_out = (uint8_t *) malloc(sizeof(uint8_t) * input_size);
 
    assert(decode_in  && "decode_in  allocation failed");
    assert(decode_out && "decode_out allocation failed");

    std::string huff_dec = inFile_name + "decode.huffman";

    // Open Encoded and Output file descriptors
    inFile  = fopen(huff_dec.c_str(), "rb");
    string outFile_name = inFile_name +  "decode.orig";
    FILE *outFile = fopen(outFile_name.c_str(), "wb");

    // Error checking
    assert(inFile && "Decode inFile fails to open");
    assert(outFile && "Decode outFile fails to open");
    
    // Read data from input file
    uint8_t c = 0;
    // Read from input file
    for(uint32_t i = 0; i < enbytes; i++){
        c = getc(inFile);
        decode_in[i] = c;
    }
    
#ifdef VERBOSE 
    printf("LZ77 Decode    -- ");
#endif
    // LZ77 decoding
    uint32_t debytes = lz77_decode(decode_in, decode_out, enbytes);
    
    for(uint32_t i = 0; i < debytes; i++)
        putc(decode_out[i], outFile); 

    // Close files
    fclose(inFile);
    fclose(outFile);
    free(decode_in);
    free(decode_out);

    return debytes;
}

void huffman_encode(const char *input) {
 
#ifdef VERBOSE 
    cout << "Huffman Encode -- "  << std::endl;
#endif   
    char inFile_name[256]; 
    strcpy(inFile_name, input);
    
    string outFile_name = inFile_name;
    outFile_name = outFile_name + "encode.lz77";

    // Huffman encoding
    char huff_cmd_encode[512];
    string huff_cmp = inFile_name;
    huff_cmp = huff_cmp + "encode.huffman";
    snprintf(huff_cmd_encode, sizeof(huff_cmd_encode), "./huffman -c -i %s -o %s",outFile_name.c_str(),huff_cmp.c_str()); 
    system(huff_cmd_encode);

}

void huffman_decode(const char *inFile_name) {
    
#ifdef VERBOSE 
    cout << "Huffman Decode -- "  << std::endl;
#endif   
    // Huffman decoding
    char huff_cmd_decode[512];
    string huff_cmp = inFile_name;
    huff_cmp = huff_cmp + "encode.huffman";
    string huff_dec = inFile_name;
    huff_dec = huff_dec + "decode.huffman";
    snprintf(huff_cmd_decode, sizeof(huff_cmd_decode), "./huffman -d -i %s -o %s",huff_cmp.c_str(),huff_dec.c_str()); 
    system(huff_cmd_decode);
}

int validate(const char *inFile_name) {


#ifdef VERBOSE 
    cout << "Validation     -- ";
#endif   
    char command[512];

    snprintf(command, sizeof(command), "cmp %s %sdecode.orig",inFile_name, inFile_name); 
    int ret = system(command);
    return ret;
}
int process(std::string & inFile_name, xil_lz77& lz77)
{
    uint32_t enbytes = lz77_encode_top(inFile_name.c_str(),lz77);
   
    // Huffman encoding
    huffman_encode(inFile_name.c_str());

    // Huffman decoding
    huffman_decode(inFile_name.c_str());

    // LZ77 decoding
    uint32_t debytes = lz77_decode_top(inFile_name.c_str(), enbytes);
    
    // Validate results
    int ret = validate(inFile_name.c_str());

    FILE *inFile = fopen(inFile_name.c_str(), "rb");
    
    // Find file size
    fseek(inFile, 0, SEEK_END);
    long input_size = ftell(inFile);
    rewind(inFile);

    fclose(inFile);    
    
    float size_in_mb = (float)input_size/1000000;
    std::cout.precision(3);
    std::cout << "\t\t" 
         << ((float)debytes/enbytes) << "\t\t"; 
    std::cout.precision(3);
    std::cout  << (ret ? "FAILED\t": "PASSED\t")<< "\t" << size_in_mb << "\t\t\t" << inFile_name << std::endl; 
    return ret;
}  
int main(int argc, char *argv[])
{
    std::string binaryFileName = "gZip_" + std::to_string(COMPUTE_UNITS) + "cu";
    xil_lz77 lz77;
    lz77.init(binaryFileName);
    
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--input_file",    "-i",       "input data flie",        "");
    parser.addSwitch("--file_list",     "-l",       "List of Input Files",    "");
    parser.parse(argc, argv);

    std::string infile      = parser.value("input_file");   
    std::string filelist    = parser.value("file_list");   
    std::cout<<"\n";
    std::cout<<"E2E(MBps)\tKT(MBps)\tCR\t\tSTATUS\t\tFile Size(MB)\t\tFile Name"<<std::endl;
    std::cout<<"\n";
    if (!filelist.empty()) {
        std::ifstream infilelist(filelist.c_str());
        std::string line;
        while(std::getline(infilelist,line)){
            process(line,lz77);
        }
    }else if (!infile.empty()){

        std::string inFile_name = argv[1];
        int ret = process(infile,lz77);   
        cout << "TEST " << (ret ? "FAILED": "PASSED") << endl; 
    }  else{
        parser.printHelp();
        exit(EXIT_FAILURE);
    }
    lz77.release();
}
