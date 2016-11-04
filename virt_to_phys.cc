#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

// ORIG_BUFFER will be placed in memory and will then be changed to NEW_BUFFER
// They must be the same length
#define ORIG_BUFFER "Hello, World!"
#define NEW_BUFFER "Hello, Linux!"

// The page frame shifted left by PAGE_SHIFT will give us the physcial address of the frame
// Note that this number is architecture dependent. For me on x86_64 with 4096 page sizes,
// it is defined as 12. If you're running something different, check the kernel source
// for what it is defined as.
#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8

void* create_buffer(void);
unsigned long get_page_frame_number_of_address(void *addr);
int open_memory(void);
void seek_memory(int fd, unsigned long offset);

int main(void) {
   // Create a buffer with some data in it
   // Check for a buffer
   void *buffer = create_buffer();
   
   //Check for Varibales
   unsigned variable = 25; // It will go to the STACK

   // Get the page frame the buffer is on
   // Getting the page frame number of the buffer
   printf("\nPrinting Page Frame Number Information:\n");
   
   unsigned int page_frame_number = get_page_frame_number_of_address(buffer);
   printf("Page frame for the buffer: 0x%x\n", page_frame_number);
   
   // Getting the page frame number of the defined variable
   unsigned int page_frame_number_variable = get_page_frame_number_of_address(&variable);
   printf("Page frame for the varibale: 0x%x\n", page_frame_number_variable);
   
   
   // Find the difference from the buffer to the page boundary
   unsigned int distance_from_page_boundary = (unsigned long)buffer % getpagesize();

   // Find the difference from the variable to the page boundary
   unsigned int variable_distance_from_page_boundary = (unsigned long)(&variable) % getpagesize();
   
   // Determine how far to seek into memory to find the buffer
   uint64_t offset = (page_frame_number << PAGE_SHIFT) + distance_from_page_boundary;
   
   // Determine how far to seek into memory to find the vairable
   uint64_t variable_offset = (page_frame_number_variable << PAGE_SHIFT) + variable_distance_from_page_boundary;

   // Open /dev/mem, seek the calculated offset, and
   // map it into memory so we can manipulate it
   // CONFIG_STRICT_DEVMEM must be disabled for this
   int mem_fd = open_memory();
   seek_memory(mem_fd, offset);
   
   // Seeking the calculated offset for the variable
   int variable_mem_fd = open_memory();
   seek_memory(variable_mem_fd, variable_offset);

   printf("\nPrinting Data Information:\n");
   
   printf("Buffer: %s\n", buffer);
   //puts("Changing buffer through /dev/mem...");

   // Printing Variable value
   printf("Variable: %d\n", variable);
   //puts("Changing buffer through /dev/mem...");
   // Change the contents of the buffer by writing into /dev/mem
   // Note that since the strings are the same length, there's no purpose in
   // copying the NUL terminator again
   
   /*if(write(mem_fd, NEW_BUFFER, strlen(NEW_BUFFER)) == -1) {
      fprintf(stderr, "Write failed: %s\n", strerror(errno));
   }*/
   
   //printf("Buffer: %s\n", buffer);
   
   // Physical Address Calculations for the buffer
   unsigned physical_addr = (page_frame_number << PAGE_SHIFT) + distance_from_page_boundary;
   
   // Physical Address Caluclations for the Variable
   unsigned physical_addr_variable = (page_frame_number_variable << PAGE_SHIFT) + variable_distance_from_page_boundary;
  
   printf("\nPrinting Buffer Information:\n");
   printf("Virtual Address is: %p\n", (void*)(buffer)); 
   printf("Physical Address is: %p\n", (void*)(physical_addr));

   printf("\nPrinting Variable Information:\n");
   printf("Virtual Address of the Variable is: %p\n", (void*)(&variable));
   printf("Physical Address of the Variable is: %p\n", (void*)(physical_addr_variable)); 
   printf("\n");
   // Clean up
   free(buffer);
   close(mem_fd);

   return 0;
}

void* create_buffer(void) {
   size_t buf_size = strlen(ORIG_BUFFER) + 1;

   // Allocate some memory to manipulate
   char *buffer = (char*) malloc(buf_size); // It will go to HEAP
   if(buffer == NULL) {
      fprintf(stderr, "Failed to allocate memory for buffer\n");
      exit(1);
   }

   // Lock the page in memory
   // Do this before writing data to the buffer so that any copy-on-write
   // mechanisms will give us our own page locked in memory
   if(mlock(buffer, buf_size) == -1) {
      fprintf(stderr, "Failed to lock page in memory: %s\n", strerror(errno));
      exit(1);
   }

   // Add some data to the memory
   strncpy(buffer, ORIG_BUFFER, strlen(ORIG_BUFFER));

   return buffer;
}

unsigned long get_page_frame_number_of_address(void *addr) {
   // Open the pagemap file for the current process
   FILE *pagemap = fopen("../../../..//proc/self/pagemap", "rb");

   // Seek to the page that the buffer is on
   unsigned long offset = (unsigned long)addr / getpagesize() * PAGEMAP_LENGTH;
   if(fseek(pagemap, (unsigned long)offset, SEEK_SET) != 0) {
      fprintf(stderr, "Failed to seek pagemap to proper location\n");
      exit(1);
   }

   // The page frame number is in bits 0-54 so read the first 7 bytes and clear the 55th bit
   unsigned long page_frame_number = 0;
   fread(&page_frame_number, 1, PAGEMAP_LENGTH-1, pagemap);

   page_frame_number &= 0x7FFFFFFFFFFFFF;

   fclose(pagemap);

   return page_frame_number;
}

int open_memory(void) {
   // Open the memory (must be root for this)
   int fd = open("/dev/mem", O_RDWR);

   if(fd == -1) {
      fprintf(stderr, "Error opening /dev/mem: %s\n", strerror(errno));
      exit(1);
   }

   return fd;
}

void seek_memory(int fd, unsigned long offset) {
   unsigned pos = lseek(fd, offset, SEEK_SET);

   if(pos == -1) {
      fprintf(stderr, "Failed to seek /dev/mem: %s\n", strerror(errno));
      exit(1);
   }
}
