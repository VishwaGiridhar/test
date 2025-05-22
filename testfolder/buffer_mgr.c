#include<stdio.h>
#include<stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <math.h>
#include "const.h"

// "bufferSize" represents the size of the buffer pool i.e. maximum number of page frames that can be kept into the buffer pool
int bufferSize = 0;

// "rearIndex" basically stores the count of number of pages read from the disk.
// "rearIndex" is also used by FIFO function to calculate the frontIndex i.e.
int rearIndex = 0;

// "writeCount" counts the number of I/O write to the disk i.e. number of pages writen to the disk
int writeCount = 0;

// "hit" a general count which is incremented whenever a page frame is added into the buffer pool.
// "hit" is used by LRU to determine least recently added page into the buffer pool.
int hit = 0;

// "clockPointer" is used by CLOCK algorithm to point to the last added page in the buffer pool.
int clockPointer = 0;

// "lfuPointer" is used by LFU algorithm to store the least frequently used page frame's position. It speeds up operation  from 2nd replacement onwards.
int lfuPointer = 0;


/*
	- description : Creates and initializes a buffer pool with page frames (numPages).
	- param :
		1. b_mgr - pointer to the buffer pool
		2. pageFN -  stores the no of page files which are cached in memory.
		3. numPages - no. of the page frames
		4. strategy -  the page replacement strategy (FIFO, LRU, LFU, CLOCK)
		5. stratData -  parameters to the page replacement strategy
	- return : RC code
*/
extern RC initBufferPool(BM_BufferPool *const b_mgr, const char *const pageFN, 
		  const int numPages, ReplacementStrategy strategy, 
		  void *stratData)
{
	b_mgr->pageFile = (char *)pageFN;
	b_mgr->numPages = numPages;
	b_mgr->strategy = strategy;
	
	// Allocate memory for page frames
	PageFrame *mypage = malloc(sizeof(PageFrame) * numPages);

	bufferSize = numPages;	
	int k = bufferSize;

	// Initialize each page frame with default values
	while(k > 0)
	{
		k--;
		mypage[k].refNum = 0;
		mypage[k].dirtyBit = 0;
		mypage[k].data = NULL;
		mypage[k].hitNum = 0;	
		mypage[k].pageNum = -1;
		mypage[k].fixCount = 0;
	}
	// Set the management data of the buffer pool to the allocated page frames
	b_mgr->mgmtData = mypage;
	// Initialize variables related to the replacement strategy
	lfuPointer = writeCount = clockPointer = 0;
	// Return success code
	return RC_OK;		
}


/*
 * Function: FIFO
 * --------------
 * Implements the First-In-First-Out (FIFO) page replacement strategy.
 * Finds the next available page frame using the circular buffer and
 * replaces its content with the incoming page. If the replaced page
 * is dirty, persists it before the replacement.
 *
 * Parameters:
 * - bm: A pointer to the buffer pool structure.
 * - page: A pointer to the page frame to be inserted/replaced in the buffer pool.
 */
void FIFO(BM_BufferPool *const bm, PageFrame *page)
{
    PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
	// Find the front index of the circular buffer
    int frontIndex = rearIndex % bufferSize;
    // Move to the next available page frame with fixCount = 0
    while (pageFrame[frontIndex].fixCount > 0)
    {
        frontIndex = (frontIndex + 1) % bufferSize;
    }
	// If the page being replaced is dirty, persist it before replacement
    if (pageFrame[frontIndex].dirtyBit == 1) {
    	persistPage(bm, &pageFrame[frontIndex]);
	}
	// Update the page frame with the incoming page
	updatePageFrame(&pageFrame[frontIndex], page);
    // Replace the content of the selected page frame with the incoming page
    pageFrame[frontIndex] = *page;
}


/*
 * Function: updatePageFrame
 * -------------------------
 * Updates the content of a destination page frame with the data from a source page frame.
 *
 * Parameters:
 * - destination: A pointer to the destination page frame to be updated.
 * - source: A pointer to the source page frame containing the data to be copied.
 */
void updatePageFrame(PageFrame *destination, const PageFrame *source) {
    // Copy the content of the source page frame to the destination page frame
	*destination = *source;
}


/*
 * Function: persistPage
 * ---------------------
 * Persists the content of a dirty page frame to the page file.
 *
 * Parameters:
 * - bm: A pointer to the buffer pool structure.
 * - pageFrame: A pointer to the page frame containing the data to be persisted.
 */
void persistPage(BM_BufferPool *const bm, PageFrame *pageFrame) {
    SM_FileHandle fh;
	// Open the page file associated with the buffer pool
    openPageFile(bm->pageFile, &fh);
	// Write the data of the dirty page frame to the page file
    writeBlock(pageFrame->pageNum, &fh, pageFrame->data);
	// Increment the global write count
    writeCount++;
}



/*
 * Function: LFU
 * ------------
 * Implements the Least Frequently Used (LFU) page replacement strategy.
 * Finds the page frame with the least reference count for replacement,
 * persisting it if necessary. Updates the chosen page frame with the
 * content of the incoming page.
 *
 * Parameters:
 * - b_mgr: A pointer to the buffer pool structure.
 * - page: A pointer to the page frame to be inserted/replaced in the buffer pool.
 */
void LFU(BM_BufferPool *const b_mgr, PageFrame *page)
{
	// Check if the buffer pool is NULL
	if (b_mgr == NULL) {
        
        return ;
    }
	PageFrame *pageFrame = (PageFrame *) b_mgr->mgmtData;
	
	// Initialize variables for LFU replacement
	int leastFreqRef;
	// Return if the incoming page is NULL
	if(page == NULL){
		  return ; 
	}
	int leastFreqIndex = 0;
	leastFreqIndex = lfuPointer;	
	int i = 0;
	int j = 0;
    leastFreqRef = pageFrame[0].refNum;
	// Find the least frequently used page frame with fixCount = 0
	for(i=0;i<bufferSize;i++) {
		if (pageFrame[leastFreqIndex].fixCount == 0) {
			int x=leastFreqIndex+i;
			leastFreqIndex = x % bufferSize;
			leastFreqRef = pageFrame[leastFreqIndex].refNum;
			break;
		}
	}
	int y = leastFreqIndex + 1;
	i = y % bufferSize;
	// Compare reference counts to find the least frequently used page frame
	for(j=0;j<bufferSize;j++) {
		if (pageFrame[i].refNum < leastFreqRef) {
			leastFreqIndex = i;
			leastFreqRef = pageFrame[i].refNum;
		}
		i = (i + 1) % bufferSize;
	}
	// Persist the chosen page frame if it is not dirty
	if (!(pageFrame[leastFreqIndex].dirtyBit)) {
		SM_FileHandle fh;
		openPageFile(b_mgr->pageFile, &fh);
		writeBlock(pageFrame[leastFreqIndex].pageNum, &fh, pageFrame[leastFreqIndex].data);
		// Increment the global write count
		writeCount++;
	}
	// Update the LFU pointer
	lfuPointer = leastFreqIndex + 1;
	// Update the chosen page frame with the content of the incoming page
	pageFrame[leastFreqIndex].fixCount = page->fixCount;	
	pageFrame[leastFreqIndex].data = page->data;
	pageFrame[leastFreqIndex].dirtyBit = page->dirtyBit;
	pageFrame[leastFreqIndex].pageNum = page->pageNum;
	
}

/*
 * Function: LRU
 * ------------
 * Implements the Least Recently Used (LRU) page replacement strategy.
 * Finds the page frame with the least recently used hit number for replacement.
 * Updates the chosen page frame with the content of the incoming page.
 * Persists the chosen page frame if it is dirty.
 *
 * Parameters:
 * - b_mgr: A pointer to the buffer pool structure.
 * - page: A pointer to the page frame to be inserted/replaced in the buffer pool.
 */
void LRU(BM_BufferPool *const b_mgr, PageFrame *page) {	
	// Check if the buffer pool is NULL
	if (b_mgr == NULL) {
        
        return;
    }
    // Check if the page file is NULL
    if (b_mgr->pageFile == NULL) {
        return;
    }
	
	
	PageFrame *pageFrame = (PageFrame *) b_mgr->mgmtData;
	int leastHitIndex = 0, leastHitNum = 0;
	// Return if the incoming page is NULL or has no data
	if(page == NULL){
		return;
	}
    if (page->data == NULL) {
        return;
    }
	int i = 0;
	// Find the page frame with the least recently used hit number and fixCount = 0
	while (i < bufferSize) {
    if (pageFrame[i].fixCount == 0) {
        leastHitIndex = i;
        leastHitNum = pageFrame[i].hitNum;
        break;
    }
   	 i++;
	}

	 i = leastHitIndex + 1;
	 leastHitIndex = 0;
	 leastHitNum = pageFrame[0].hitNum;
	 i = 1;
	// Compare hit numbers to find the least recently used page frame
	while (i < bufferSize) {
		if (!(pageFrame[i].hitNum >= leastHitNum)) {
			leastHitIndex = i;
			leastHitNum = pageFrame[leastHitIndex].hitNum;
		}
		i=i+1;
	}
	// Update the chosen page frame with the content of the incoming page
	pageFrame[leastHitIndex].dirtyBit = page->dirtyBit;
	pageFrame[leastHitIndex].hitNum = page->hitNum;
	pageFrame[leastHitIndex].data = page->data;
	pageFrame[leastHitIndex].dirtyBit = page->dirtyBit;
	pageFrame[leastHitIndex].pageNum = page->pageNum;
	pageFrame[leastHitIndex].fixCount = page->fixCount;
	// Persist the chosen page frame if it is dirty
	if (!(pageFrame[leastHitIndex].dirtyBit != 1)){
		SM_FileHandle fh;
		openPageFile(b_mgr->pageFile, &fh); 
		writeBlock(pageFrame[leastHitIndex].pageNum, &fh, pageFrame[leastHitIndex].data); // Write the data
		// Write the data
		writeCount++;
	}
}

/*
 * Function: LRU_K
 * ---------------
 * Implements the Least Recently Used with K-second aging (LRU-K) page replacement strategy.
 * Finds the page frame with the least recently used hit number for replacement.
 * Updates the chosen page frame with the content of the incoming page.
 * Persists the chosen page frame if it is dirty.
 *
 * Parameters:
 * - b_mgr: A pointer to the buffer pool structure.
 * - page: A pointer to the page frame to be inserted/replaced in the buffer pool.
 */
void LRU_K(BM_BufferPool *const b_mgr, PageFrame *page) {
   // Check if the buffer pool is NULL
   if (b_mgr == NULL) {
        return;
    }
    // Check if the page file is NULL
    if (b_mgr->pageFile == NULL) {
        
        return;
    }
    
	
	
	PageFrame *pageFrame = (PageFrame *) b_mgr->mgmtData;
	int leastHitIndex = 0, leastHitNum = 0;
	// Return if the incoming page is NULL or has no data
	if(page == NULL){
		return;
	}
    
    if (page->data == NULL) {
       
        return;
    }
	
	int i = 0;
	// Find the page frame with the least recently used hit number and fixCount = 0
	while (i < bufferSize) {
    if (pageFrame[i].fixCount == 0) {
        leastHitIndex = i;
        leastHitNum = pageFrame[i].hitNum;
        break;
    }
    i++;
}
	// Compare hit numbers to find the least recently used page frame
	for(i=leastHitIndex + 1; i<bufferSize; i++) {
		if (!(pageFrame[i].hitNum >= leastHitNum)) {  
			leastHitIndex  = i;
			leastHitNum = pageFrame[i].hitNum;
		}
	}
	// Update the chosen page frame with the content of the incoming page
	pageFrame[leastHitIndex].hitNum = page->hitNum;
	pageFrame[leastHitIndex].data = page->data;
	pageFrame[leastHitIndex].pageNum = page->pageNum;
	pageFrame[leastHitIndex].fixCount = page->fixCount;
	pageFrame[leastHitIndex].dirtyBit = page->dirtyBit;
	// Persist the chosen page frame if it is dirty
	if (pageFrame[leastHitIndex].dirtyBit) {
		SM_FileHandle fh;
		openPageFile(b_mgr->pageFile, &fh);
		writeBlock(pageFrame[leastHitIndex].pageNum, &fh, pageFrame[leastHitIndex].data); // Write the data
		writeCount++;
	}
}


/*
 * Function: CLOCK
 * ---------------
 * Implements the CLOCK page replacement strategy.
 * Iterates through the circular buffer using a clock hand to find the next
 * available page frame. If a suitable page frame is found, updates it with
 * the content of the incoming page. Persists the replaced page if it is dirty.
 *
 * Parameters:
 * - b_mgr: A pointer to the buffer pool structure.
 * - page: A pointer to the page frame to be inserted/replaced in the buffer pool.
 */
void CLOCK(BM_BufferPool *const b_mgr, PageFrame *page) {    
    PageFrame *pageFrame = (PageFrame *) b_mgr->mgmtData;
	 // Iterate through the circular buffer using a clock hand
    for (; clockPointer % bufferSize != 0; clockPointer++) {
        // If an available page frame is found
		if (!(pageFrame[clockPointer].hitNum)) {
			// If the found page frame is dirty, persist it
            if (pageFrame[clockPointer].dirtyBit) {  
                SM_FileHandle fh;
                openPageFile(b_mgr->pageFile, &fh); 
                writeBlock(pageFrame[clockPointer].pageNum, &fh, pageFrame[clockPointer].data); // and write the data
            }
            // Update the found page frame with the content of the incoming page
            pageFrame[clockPointer] = *page;
            break;
        } else {
            // Reset the hitNum of the examined page frame
            pageFrame[clockPointer].hitNum = 0;
        }
    }
     // Move the clock hand to the next position in the circular buffer
    clockPointer = (clockPointer + 1) % bufferSize;
}



/*
 * Function: shutdownBufferPool
 * ----------------------------
 * Shuts down and releases resources associated with the buffer pool.
 * Checks for any pinned pages in the buffer pool and returns an error if found.
 * Forces the flush of all dirty pages before freeing the allocated memory.
 *
 * Parameters:
 * - b_mgr: A pointer to the buffer pool structure to be shutdown.
 *
 * Returns:
 * - RC_OK: If the buffer pool is successfully shutdown.
 * - RC_BUFFER_POOL_SHUTDOWN_ERROR: If there is an issue with the buffer pool shutdown.
 * - RC_PINNED_PAGES_IN_BUFFER: If there are pinned pages in the buffer pool.
 */
RC shutdownBufferPool(BM_BufferPool *const b_mgr) {
     // Check if the management data is NULL
	if (b_mgr->mgmtData == NULL) {
        return RC_BUFFER_POOL_SHUTDOWN_ERROR;
    }

    PageFrame *pageFrame;
    pageFrame = (PageFrame *) b_mgr->mgmtData;

    // Check for pinned pages in the buffer pool
    for (int i = 0; i < bufferSize; i++) {
        if (pageFrame[i].fixCount != 0) {
            return RC_PINNED_PAGES_IN_BUFFER;
        }
    }
    // Force flush all dirty pages before freeing the allocated memory
    forceFlushPool(b_mgr);
    // Free the allocated memory for page frames
    free(pageFrame);
	// Set the management data to NULL
    b_mgr->mgmtData = NULL;
    // Return success code
    return RC_OK;
}





/*
 * Function: forceFlushPool
 * ------------------------
 * Forces the flush of all dirty pages in the buffer pool to the page file.
 * Checks for errors and returns an error code if encountered.
 *
 * Parameters:
 * - bPool: A pointer to the buffer pool structure.
 *
 * Returns:
 * - RC_OK: If the force flush operation is successful.
 * - RC_ERROR: If there is an issue with the force flush operation.
 */
RC forceFlushPool(BM_BufferPool* const bPool) {
    if (bPool->mgmtData == NULL) {
        return RC_ERROR;
    }
    PageFrame *pFrames = (PageFrame*)bPool->mgmtData;
    // Create a list to store pages that need to be written
    int *pagesToWrite = malloc(sizeof(int) * bPool->numPages);
    int numPagesToWrite = 0;
    // Check if the page is dirty and if no one is touching it, add it to the list
    for (int i = 0; i < bPool->numPages; i++) {
        if (pFrames[i].fixCount == 0) {
			if(pFrames[i].dirtyBit == 1){
				pagesToWrite[numPagesToWrite++] = pFrames[i].pageNum;
				pFrames[i].dirtyBit = 0;
				writeCount++;
			}
        }
    }
    // Open the disk file outside the loop
    SM_FileHandle fileHandle;
    openPageFile(bPool->pageFile, &fileHandle);
    // Write the pages from the list
    for (int i = 0; i < numPagesToWrite; i++) {
        writeBlock(pagesToWrite[i], &fileHandle, pFrames[i].data);
    }
    // Free the allocated memory for the list
    free(pagesToWrite);

    return RC_OK;
}



/*
 * Function: markDirty
 * -------------------
 * Marks the specified page in the buffer pool as dirty.
 *
 * Parameters:
 * - b_mgr: A pointer to the buffer pool structure.
 * - page: A pointer to the page handle structure containing the page number.
 *
 * Returns:
 * - RC_OK: If the page is successfully marked as dirty.
 * - RC_ERROR: If the specified page is not found in the buffer pool.
 */
RC markDirty(BM_BufferPool *const b_mgr, BM_PageHandle *const page)
{
	// Retrieve the page frames from the buffer pool
    PageFrame *pageFrame = (PageFrame *)b_mgr->mgmtData;

    // Iterate through the page frames to find the specified page
    for (int i = 0; i < bufferSize; i++)
    {
        // If the page is found, mark it as dirty and return success
        if (pageFrame[i].pageNum == page->pageNum)
        {
            pageFrame[i].dirtyBit = 1;
            return RC_OK;
        }
    }
	// Return error code if the specified page is not found
    return RC_ERROR;
}


/*
	- Description: Removes the page from memory by decrementing its fix count (unpin the page).
	- Parameters:
		1. b_mgr - Pointer to the buffer pool.
		2. page - Pointer to the BM_PageHandle structure representing the page to be unpinned.
	- Return: RC code indicating the result of the operation.
*/
RC unpinPage(BM_BufferPool *const b_mgr, BM_PageHandle *const page)
{
	// Check if the buffer pool is open
     if (b_mgr->mgmtData == NULL) {
        return RC_POOL_NOT_OPEN;
    }
	// Retrieve the page frames from the buffer pool
    PageFrame *frameOfPage = (PageFrame *)b_mgr->mgmtData;
    bool flag = false;
   // Iterate through the page frames to find the specified page
    for (int j = 0; j < b_mgr->numPages; j++)
    {
       // Check if the current page is the page to be unpinned
        if (frameOfPage[j].pageNum == page->pageNum)
        {
            flag = true;
             // Ensure fix count doesn't go below 0
            if (frameOfPage[j].fixCount > 0) {
                frameOfPage[j].fixCount--;
            } else {
                
                return RC_PAGE_NOT_PINNED;
            }
            break; 
        }
    }
    if(flag == false){
        return RC_PAGE_NOT_IN_FRAMELIST;
    }

    return RC_OK;
}

/*
	- Description: Writes the current content of the page back to the page file on disk.
	- Parameters:
		1. b_mgr - Pointer to the buffer pool.
		2. page - Pointer to the BM_PageHandle structure representing the page to be written.
	- Return: RC code indicating the result of the operation.
*/
RC forcePage(BM_BufferPool *const b_mgr, BM_PageHandle *const page)
{
    PageFrame *pageFrame = (PageFrame *)b_mgr->mgmtData;
        // Iterating through all the pages in the buffer pool
    for (int i = 0; i < bufferSize; i++)
    {
       // If the current page matches the page to be written to disk, write the page to the disk using the storage manager functions
        if (!(pageFrame[i].pageNum != page->pageNum))

        {
            SM_FileHandle fh;
            openPageFile(b_mgr->pageFile, &fh);
            writeBlock(pageFrame[i].pageNum, &fh, pageFrame[i].data);
           // Mark the page as undirty because the modified page has been written to disk
            pageFrame[i].dirtyBit = 0;
          // Increase the writeCount which records the number of writes done by the buffer manager.
            writeCount++;
            return RC_OK;
        }
    }
    return RC_PAGE_NOT_IN_FRAMELIST;
}

/*
	- Description: Checks if the page frame is empty.
	- Parameters:
		1. frame - Pointer to the PageFrame structure.
	- Return: Boolean value indicating whether the page frame is empty.
*/
bool isPageFrameEmpty(const PageFrame *frame){
		return frame->pageNum == -1;
	}
/*
	- Description: Pins the page with the given page number in the buffer pool, replacing a page if necessary.
	- Parameters:
		1. b_mgr - Pointer to the buffer pool structure.
		2. page - Pointer to the BM_PageHandle structure for storing page information.
		3. pageNum - Page number to be pinned.
	- Return: RC_OK if successful, or corresponding error codes.
*/
RC pinPage (BM_BufferPool *const b_mgr, BM_PageHandle *const page, const PageNumber pageNum)
{
     if (b_mgr->mgmtData == NULL) {
        return RC_PAGE_NOT_PINNED;
    }
	 // Check if the page number is negative
    if(pageNum<0){
        return RC_NEGATIVE_PAGE_NUM;
    }

	PageFrame *frameOfPage = (PageFrame *)b_mgr->mgmtData;

	 // Check if the buffer pool is not empty
	if(!isPageFrameEmpty(&frameOfPage[0]))
	{	
		bool bufferFull = true;
		int j;
		j = 0;
		// Iterate through the buffer pool pages
		while (j < bufferSize) {
			// Check if the page frame is empty

			if (frameOfPage[j].pageNum == -1) {
				// Initialize a new page frame for the page to be pinned

    SM_FileHandle fileh;
	frameOfPage[j].data = (SM_PageHandle)malloc(PAGE_SIZE);
	openPageFile(b_mgr->pageFile, &fileh);
	readBlock(pageNum, &fileh, frameOfPage[j].data);

	rearIndex++;
	hit++; // Increase the hit (LRU algorithm uses the hit to find the least recently used page)

	page->pageNum = pageNum;
	page->data = frameOfPage[j].data;

	bufferFull = false;
	// Set hitNum based on the page replacement strategy
	if (b_mgr->strategy == RS_CLOCK)
	// hitNum is set to 1 to signify that this was the final page frame checked before adding it to the buffer.
		frameOfPage[j].hitNum = 1;
	else if (b_mgr->strategy == RS_LRU)
	// The least recently used page is determined by the LRU algorithm using the hit value.
		frameOfPage[j].hitNum = hit;

	frameOfPage[j].refNum = 0;
	frameOfPage[j].pageNum = pageNum;
	frameOfPage[j].fixCount = 1;

		break;
	} else {
    // Verifying that the page is in memory
    if (frameOfPage[j].pageNum == pageNum) {
                          // Update fixCount as a new client has just accessed this page
        bufferFull = false;
        frameOfPage[j].fixCount++;
        hit++; // Increasing the hit (the LRU method uses the hit to find the least recently used page).

        switch (b_mgr->strategy) {
            case RS_CLOCK:
			// hitNum is set to 1 to signify that this was the final page frame checked before adding it to the buffer pool.
                frameOfPage[j].hitNum = 1;
                break;
            case RS_LFU:
             // Increase reference count, representing one more usage of the page (referenced).
                frameOfPage[j].refNum++;
                break;
            case RS_LRU:
            case RS_LRU_K:
                // The least recently used page is determined by the LRU algorithm using the hit value.
                frameOfPage[j].hitNum = hit;
                break;
            default:
                // Handle the case where the strategy is not recognized
                break;
        }

        clockPointer++;
        page->data = frameOfPage[j].data;
        page->pageNum = pageNum;

        break;
    }
}
			j++;
		}
        // Check if the buffer is full
		bool bufferFullCondition = bufferFull == true;
	if (bufferFullCondition) {
 	// Create a new page frame to store data read from the file.
    PageFrame *newPage = (PageFrame *)malloc(sizeof(PageFrame));
    // Read a page from disk and start the buffer pool with the contents of the page frame
    SM_FileHandle fileH;
    openPageFile(b_mgr->pageFile, &fileH);
    newPage->data = (SM_PageHandle)malloc(PAGE_SIZE);
	newPage->refNum = 0;
    newPage->fixCount = 1;
    readBlock(pageNum, &fileH, newPage->data);
    newPage->pageNum = pageNum;
    newPage->dirtyBit = 0;
    hit++;
    rearIndex++;
    // Set hitNum based on the page replacement strategy
    if (b_mgr->strategy == RS_LRU_K || b_mgr->strategy == RS_LRU) {
        newPage->hitNum = hit;
    } else if (b_mgr->strategy == RS_CLOCK) {
        newPage->hitNum = 1;
    }
    page->pageNum = pageNum;
    page->data = newPage->data;
	 // Depending on the chosen page replacement technique, call the relevant algorithm's function (provided through arguments).
			switch (b_mgr->strategy) {
				case RS_FIFO:
					FIFO(b_mgr, newPage);
					break;
				case RS_LRU:
					LRU(b_mgr, newPage);
					break;
				case RS_CLOCK:
					CLOCK(b_mgr, newPage);
					break;
				case RS_LFU:
					LFU(b_mgr, newPage);
					break;
				case RS_LRU_K:
					LRU_K(b_mgr, newPage);
					break;
				default:
					printf("\nNo algorithm has been used.\n");
			}

            free(newPage);

		}		
		return RC_OK;
	}else{
		// Buffer pool is empty, initialize the buffer pool with the first page
		SM_FileHandle fh;
		openPageFile(b_mgr->pageFile, &fh);

		SM_PageHandle pageData = (SM_PageHandle)malloc(PAGE_SIZE);
		frameOfPage[0].data = pageData;

		frameOfPage[0].pageNum = pageNum;
		rearIndex = 0;

		ensureCapacity(pageNum, &fh);
		readBlock(pageNum, &fh, frameOfPage[0].data);

		// Reset some attributes
		hit = 0;
		frameOfPage[0].fixCount++;
		frameOfPage[0].hitNum = hit;
		frameOfPage[0].refNum = 0;

		page->pageNum = pageNum;
		page->data = frameOfPage[0].data;
    	// Reset some attributes
		return RC_OK;

	}	
}

/*
	- Description: Returns an array of page numbers corresponding to the pages currently in the buffer pool.
	- Parameters:
		1. b_mgr - Pointer to the buffer pool structure.
	- Return: PageNumber array containing page numbers of pages in the buffer pool.
*/

PageNumber *getFrameContents(BM_BufferPool *const b_mgr) {
	// Allocate memory for an array to store page numbers of pages in the buffer pool
    PageNumber *frameContents = malloc(sizeof(PageNumber) * bufferSize);
	// Access the array of page frames from buffer pool management data
    PageFrame *pageFrame = (PageFrame *)b_mgr->mgmtData;

    int i = 0;

   // Iterate through all pages in the buffer pool and retrieve their page numbers
    while (i < bufferSize) {
		// Set frameContents array with the page number of each page, treating -1 as NO_PAGE
        frameContents[i] = (pageFrame[i].pageNum != -1) ? pageFrame[i].pageNum : NO_PAGE;
        i++;
    }
 // Return the array of page numbers
    return frameContents;
}

/*
	- description :Function to retrieve an array of boolean values representing dirty flags for pages in the buffer pool
	- param :
		1. b_mgr - pointer to the buffer pool
	- return : boolean
*/
bool *getDirtyFlags(BM_BufferPool *const b_mgr) {
	    // Access the array of page frames from buffer pool management data
    PageFrame *pageFrame = (PageFrame *)b_mgr->mgmtData;
    // Allocate memory to store dirty flags for each page in the buffer pool
    bool *dirtyFlags = malloc(sizeof(bool) * bufferSize);
 // Iterate through all pages in the buffer pool and retrieve their dirty flags
    for (int i = 0; i < bufferSize; i++) {
// Set dirtyFlags array with TRUE if page is dirty, else set it to FALSE
        dirtyFlags[i] = (pageFrame[i].dirtyBit == 1) ? true : false;
    }
    // Return the array of dirty flags

    return dirtyFlags;
}

/*
    - Description: Retrieves an array containing fix counts for each page frame in the buffer pool.
    - Param:
        1. b_mgr - Pointer to the buffer pool structure (BM_BufferPool).
    - Return: A dynamically allocated integer array representing fix counts for each page frame.
              It is the caller's responsibility to free the allocated memory.
*/
int *getFixCounts(BM_BufferPool *const b_mgr) {
    // Access the PageFrame array from the buffer pool management data
    PageFrame *pageFrame = (PageFrame *)b_mgr->mgmtData;

    // Allocate memory for an array of int to store fix counts for each page frame
    int *fixCounts = malloc(sizeof(int) * bufferSize);

    int i = 0;
    // Iterate through all the pages in the buffer pool and set fixCounts' value to the page's fixCount
    while (i < bufferSize) {
        // Store fixCount, treating -1 as 0 (since -1 indicates an uninitialized fixCount)
        fixCounts[i] = (pageFrame[i].fixCount != -1) ? pageFrame[i].fixCount : 0;
        i++;
    }

    // Return the array of fix counts
    return fixCounts;
}


/*
    - Description: Retrieves the number of read I/O operations performed since the initialization of the buffer pool.
    - Param:
        1. b_mgr - Pointer to the buffer pool structure (BM_BufferPool).
    - Return: An integer representing the count of read I/O operations. 
              The count is calculated as the current rear index plus one.
*/
int getNumReadIO(BM_BufferPool *const b_mgr)
{
    // The number of read I/O operations is equivalent to the current rear index plus one.
    return (rearIndex + 1);
}

/*
	 Function to retrieve the total number of write operations performed by the buffer manager.
	 The count is maintained as a global variable and incremented each time a write operation is executed.
	 Parameters:
	   - b_mgr: Buffer pool structure pointer representing the buffer manager.
	 Returns:
	   - Integer value representing the total number of write operations.
*/
int getNumWriteIO (BM_BufferPool *const b_mgr)
{
	// Return the global variable holding the count of write operations.
	return writeCount;
}
