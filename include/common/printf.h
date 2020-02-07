// NOTE to the CS 452 TAs:
// See https://piazza.com/class/k4yiinhftu3b1?cid=74

///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2014-2019, PALANDesign Hannover, Germany
//
// \license The MIT License (MIT)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// \brief Tiny printf, sprintf and snprintf implementation, optimized for speed on
//        embedded systems with a very limited resources.
//        Use this instead of bloated standard/newlib printf.
//        These routines are thread safe and reentrant.
//
///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! //
// !!!!!!!!!!!!!!!!!!! CS 452 MODIFICATIONS !!!!!!!!!!!!!!!!!!! //
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! //
//////////////////////////////////////////////////////////////////
//
// The key functionality we wanted from this library was the `fctprintf`
// function, as it enables haivng a common implementation of bwprintf, kprintf,
// and UART server backed printf.
//
// Moreover, for our use case, there isn't really a sane "default" printf.
// As such, we've modified this header file to avoid re-exporting it's printf
// method, ensuring that if it's accidentally used, then libc's printf will be
// called (resulting in an abort).

#ifndef _PRINTF_H_
#define _PRINTF_H_

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Output a character to a custom device like UART, used by the printf() function
 * This function is declared here only. You have to write your custom implementation somewhere
 * \param character Character to output
 */
void _putchar(char character);

/**
 * vprintf with output function
 * You may use this as dynamic alternative to vprintf() with its fixed _putchar() output
 * \param out An output function which takes one character and an argument pointer
 * \param arg An argument pointer for user data passed to output function
 * \param format A string that specifies the format of the output
 * \return The number of characters that are sent to the output function, not counting the terminating null character
 */
int vfctprintf(void (*out)(char character, void* arg), void* arg, const char* format, va_list va);

/**
 * printf with output function
 * You may use this as dynamic alternative to printf() with its fixed _putchar() output
 * \param out An output function which takes one character and an argument pointer
 * \param arg An argument pointer for user data passed to output function
 * \param format A string that specifies the format of the output
 * \return The number of characters that are sent to the output function, not counting the terminating null character
 */
int fctprintf(void (*out)(char character, void* arg), void* arg, const char* format, ...);


#ifdef __cplusplus
}
#endif


#endif  // _PRINTF_H_
