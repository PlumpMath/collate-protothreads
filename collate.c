/*
 * Modified Nov. 7 2015 by Paul Tarvydas to run "Collate" example on page 91
 * of Flow-Based Programmin, 2nd Edition by J. Paul Morrison.
 * Download protothreads and place this file in the protothreads directory,
 * and modify the Makefile to include collate.c
 */

/*
 * Copyright (c) 2004-2005, Swedish Institute of Computer Science.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is part of the protothreads library.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: example-buffer.c,v 1.5 2005/10/07 05:21:33 adam Exp $
 */

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include <assert.h>

#include "pt-sem.h"
 
#define NUM_HEADERS 3
#define NUM_DATA 7
#define BUFSIZE 5

static int bufferA[BUFSIZE];
static int bufferB[BUFSIZE];
static int bufferC[BUFSIZE];
static int readA=0, writeA=0;
static int readB=0, writeB=0;
static int readC=0, writeC=0;

static char* buffname (int *addr) {
  if (addr == &bufferA[0]) {
    return "A";
  } else if (addr == &bufferB[0]) {
    return "B";
  } else if (addr == &bufferC[0]) {
    return "C";
  } else
    return "?";
}

static void
add_to_buffer(int item, int *buffer, int *bufptr)
{
  //printf("Item %d added to buffer %s at place %d\n", item, buffname(buffer), *bufptr);  
  buffer[*bufptr] = item;
  *bufptr = (*bufptr + 1) % BUFSIZE;
}

static int
get_from_buffer(int *buffer, int *bufptr)
{
  int item;
  item = buffer[*bufptr];
  //printf("Item %d retrieved from buffer %s at place %d\n", item, buffname(buffer), *bufptr);
  *bufptr = (*bufptr + 1) % BUFSIZE;
  return item;
}

static struct pt_sem fullA, emptyA, fullB, emptyB, fullC, emptyC;
 
/* part A produces headers */
static
PT_THREAD(header_producer(struct pt *pt))  
{
  static int key;
  PT_BEGIN(pt);
  for (key = 1 ; key <= NUM_HEADERS ; key += 1) {
    PT_SEM_WAIT(pt, &emptyA);
    add_to_buffer(key, &bufferA[0], &writeA);
    PT_SEM_SIGNAL(pt, &fullA);
  }
  PT_SEM_WAIT(pt, &emptyA);
  add_to_buffer(-1, &bufferA[0], &writeA);
  PT_SEM_SIGNAL(pt, &fullA);
  printf ("end header\n");
  PT_EXIT(pt);
  PT_END(pt);
}

/* part B produces data - the bottom 2 bits represent the key values 1, 2 & 3 */
/* data is 4 nibbles long - topmost nibble is the key */
#define DATALEN (9+3+10)
static unsigned int data[DATALEN] = { 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1008, 0x1009,
				      0x2006, 0x2007, 0x2008,
				      0x3001, 0x3002, 0x3003, 0x3004, 0x3005, 0x3006, 0x3007, 0x3008, 0x3009, 0x3000 };
static 
PT_THREAD(data_producer(struct pt *pt))
{
  static int i;
  PT_BEGIN(pt);
  for (i = 0; i < DATALEN ; i += 1) {
    PT_SEM_WAIT(pt, &emptyB);
    add_to_buffer(data[i], &bufferB[0], &writeB);
    PT_SEM_SIGNAL(pt, &fullB);
  }
  printf ("end data\n");
  PT_EXIT(pt);
  PT_END(pt);
}
 
static 
PT_THREAD(collate(struct pt *pt))
{
  static int key = -1;   // -1 means that key is empty
  static int data = -1;  // -1 says that data is "empty", we have to wait for a key from A
  PT_BEGIN(pt);
  while (1) {
    // get a key
    PT_SEM_WAIT(pt, &fullA);
    key = get_from_buffer(&bufferA[0], &readA);
    PT_SEM_SIGNAL(pt, &emptyA);
    if (key == -1)
      break;
    /* send header to component D */
    PT_SEM_WAIT(pt, &emptyC);
    add_to_buffer(key, &bufferC[0], &writeC);
    PT_SEM_SIGNAL(pt, &fullC);
    if (data != -1) {
      //printf ("key = %x data = %x\n", key, data);
      assert (key == (0xf & (data >> 12)));
      PT_SEM_WAIT(pt, &emptyC);
      add_to_buffer(data, &bufferC[0], &writeC);
      PT_SEM_SIGNAL(pt, &fullC);
    }

    /* read data from component B until key is changed */
    while (1) {
      PT_SEM_WAIT(pt, &fullB);
      data = get_from_buffer(&bufferB[0], &readB);
      PT_SEM_SIGNAL(pt, &emptyB);
      if (key != (0xf & (data >> 12))) {
	break;  // key has changed
      }
      PT_SEM_WAIT(pt, &emptyC);
      add_to_buffer(data, &bufferC[0], &writeC);
      PT_SEM_SIGNAL(pt, &fullC);
    }
  }
  printf ("end collate\n");
  PT_SEM_WAIT(pt, &emptyC);
  add_to_buffer(-1, &bufferC[0], &writeC);
  PT_SEM_SIGNAL(pt, &fullC);
  PT_EXIT(pt);
  PT_END(pt);
}

static PT_THREAD(printer(struct pt *pt)) {
  static int data;
  static int key;
  PT_BEGIN(pt);
  while(1) {
    PT_SEM_WAIT(pt, &fullC);
    data = get_from_buffer(&bufferC[0], &readC);
    if (data == -1)
      break;
    key = 0xf & (data >> 12);
    if (key == 0) {
      printf ("result: Header %d\n", data);
    } else {
      printf ("result: Data %d %d\n", key, data &0x0fff);
    }
    PT_SEM_SIGNAL(pt, &emptyC);
  }
  printf ("end printer\n");
  PT_EXIT(pt);
  PT_END(pt);
}
      
 
static 
PT_THREAD(driver_thread(struct pt *pt))
{
  static struct pt pt_A, pt_B, pt_C, pt_D;
 
  PT_BEGIN(pt);
  
  PT_SEM_INIT(&emptyA, BUFSIZE);
  PT_SEM_INIT(&fullA, 0);
  PT_SEM_INIT(&emptyB, BUFSIZE);
  PT_SEM_INIT(&fullB, 0);
  PT_SEM_INIT(&emptyC, BUFSIZE);
  PT_SEM_INIT(&fullC, 0);
 
  PT_INIT(&pt_A);
  PT_INIT(&pt_B);
  PT_INIT(&pt_C);
  PT_INIT(&pt_D);
 
  while (PT_SCHEDULE(printer(&pt_D))) {
    PT_SCHEDULE(header_producer(&pt_A));
    PT_SCHEDULE(data_producer(&pt_B));
    PT_SCHEDULE(collate(&pt_C));
  } 
  PT_END(pt);
}


int
main(void)
{
  struct pt driver_pt;

  PT_INIT(&driver_pt);

  while(PT_SCHEDULE(driver_thread(&driver_pt))) {

    /*
     * When running this example on a multitasking system, we must
     * give other processes a chance to run too and therefore we call
     * usleep() resp. Sleep() here. On a dedicated embedded system,
     * we usually do not need to do this.
     */
#ifdef _WIN32
    Sleep(0);
#else
    usleep(10);
#endif
  }
  return 0;
}
