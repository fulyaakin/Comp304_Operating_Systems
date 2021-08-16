This project is completed by:
    Dİlara Deveci - 0068182
    Fulya Akın - 0064220

The code for part1 is compiled with
        gcc part1.c -o part1
        
It is run with                        
	./part1 BACKING_STORE.bin addresses.txt

The code for part2 is compiled with
        gcc part2.c -o part2
        
It is run with                        
	./part2 BACKING_STORE.bin addresses.txt -p 0

The zip file includes:
    part1.c,
    part2.c,
    README.txt

Brief explanation of our implementations:

Part 1
First, we set the page_mask and offset_mask to page_size - 1.
Then, we filled out the search_tlb function. It goes over the elements of the TLB and if it finds the desired page in TLB, return the physical frame of it, if it cannot find it returns -1 indicating the desired logical page is not in TLB
Next, we filled out the add_to_tlb function. It creates a tlbentry, adds it to tlb by setting the appropriate tlb entry's attributes (logical and physical) to the given ones. Also, it  increments the tlbindex as one more insert into TLB is completed
This approach is FIFO as we keep changing the logical and physical of tlb[tlbindex % TLB_SIZE], meaning the oldest element in TLB will be overwritten.
After that, in order to calculate the page offset and logical page number from logical_address, we set offset to logical address & OFFSET_MASK, so that it will represent the next offset and logical_page to (logical_address >> OFFSET_BITS) & PAGE_MASK, so that it will represent the next logical_page.
Finally, we filled out the page fault part. We first increased the page_faults number by 1 to keep track of the number of page faults. Then, we set physical_page to next unallocated physical page and incremented the free_page so that it will represent the next unallocated physical page. Then, we copied the page from physical to logical_page by using memcpy and lastly, we updated the page table accordingly by setting the appropriate entry of pagetable to the physical page (pagetable[logical_page] = physical_page;)


Part 2
Most of the parts are same as part 1’s answers. We will be explaining the different parts in details.
First, we defined page_frames as it is no longer same with pages (#define PAGE_FRAMES 256). 
Then, we updated the memory size accordingly (#define MEMORY_SIZE PAGE_FRAMES * PAGE_SIZE)
After that, just like in Part 1 the search_tlb and add_to_tlb functions are filled. Unlike part 1, in part 2 we have inputs functions in order to take the inputs from the command line and set the page replacement policy (p) to the given integer.
ALso, we changed the unsigned chars into unsigned ints as the maximum value of an unsigned char is 255 but we have 1024 pages. (the unsigned chars changed are the fields of struct tlbentry, attributes of search and add tlb and the variable free pages)
Next, in the main we called inputs and initialized the accessed table (int accessed[PAGES] = {0};). The accessed table is an array of size PAGES, it records the last time that the page is accessed in the virtual memory. The table is updated every time a page is accessed. 
Then, we calculated the page offset and logical page number just like in part 1.
After that, we check if the page replacement policy is LRU, if so, we used total addresses as a counting number in the accessed table. 
Then if there is a page fault, first we increased the page_faults number by 1 to keep track of the number of page faults and checked if the policy is FIFO. If so, we set physical_page to the next unallocated physical page and updated the free_page so that it will represent the next unallocated physical page by writing this: (free_page = (free_page + 1) % PAGE_FRAMES) as the number of page frames are not equal to the number of pages.
If it is not FIFO, then it is LRU. For LRU, we write the code so that it will place pages in the empty page frames just like in FIFO until the memory fills up for the first time. After it is filled up, we selected the victim page by choosing the least recently accessed one. We checked the accessed table to see the least recent one by iterating over the table among the pages that are currently in memory. Finally, after finding the victim and replacing it, we updated the accessed table accordingly.
