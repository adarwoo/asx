/**
 * \file
 *
 * \brief Configuration Change Protection
 *
 * Copyright (c) 2009 Microchip Technology Inc. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
#include <assembler.h>

//! Value to write to CCP for access to protected IO registers.
#define CCP_IOREG   0xd8
#define CCP_SPM     0x9d


PUBLIC_FUNCTION(ccp_write_io)

movw    r30, r24                // Load addr into Z
ldi     r18, CCP_IOREG          // Load magic CCP value
out     CCP, r18                // Start CCP handshake
st      Z, r22                  // Write value to I/O register
ret                             // Return to caller

END_FUNC(ccp_write_io)


PUBLIC_FUNCTION(ccp_write_spm)

movw    r30, r24                // Load addr into Z
ldi     r18, CCP_SPM            // Load magic CCP value
out     CCP, r18                // Start CCP handshake
st      Z, r22                  // Write value to I/O register
ret                             // Return to caller

END_FUNC(ccp_write_spm)
	
END_FILE()