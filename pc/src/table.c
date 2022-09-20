/*
 * Copyright (c) 2022, Daniel Tabor
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
int table_init(char* table) {
	*table = 0;
}

int table_scan(const char* table, char* str, unsigned int len) {
	int idx = 0;
	const char* ttable = table;
	char* tstr;
	char* estr = str+len-1;
	char tc;
	char sc;
	char match;
	while( *ttable != 0 ) {
		tstr = str;
		match = 1;
		while( 1 ) {
			tc = (*ttable)&0x7F;
			if( tc >= 'A' && tc <= 'Z' ) { tc = tc + 32; }
			sc = (*tstr)&0x7F;
			if( sc >= 'A' && sc <= 'Z' ) { sc = sc + 32; }
			if( tc != sc ) { match = 0; break; }
			tc = (*ttable)&0x80;
			sc = tstr==estr;
			if( tc && sc ) { break; }
			if( tc || sc ) { match = 0; break; }
			ttable++;
			tstr++;
		}
		if( match ) {
			return idx;
		}
		while( ((*ttable)&0x80) == 0 ) {
			ttable++;
		}
		ttable++;
		idx++;
	}
	return -1;
}

char* table_next(char* ptr) {
	while( 1 ) {
		if( *ptr == 0 )
			return ptr;
		else if( (*ptr & 0x80) != 0 )
			return ptr+1;
		else 
			ptr++;
	}
}

char* table_add(char* table, unsigned int maxsize, char* str, unsigned int len, int term) {
	char* entry = table;
	char* ttable;
	char* tstr;
	while( *entry != 0 ) entry++;

	ttable = entry;
	tstr = str;
	if( term ) {
		term = 1;
	}
	while( 1 ) {
		//New entry is too big for table
		if( ttable+term >= table+maxsize ) {
			*entry = 0;
			entry = 0;
			break;
		}
		//Length is specified and we are adding the last character
		//Length was not specified or we've hit a null byte
		else if( (len && ((tstr-str) == len)) || *str == 0 ) {
			if( term ) {
				*ttable = 0x80;
				*(ttable+1) = 0;
			} else {
				*(ttable-1) = *(ttable-1) | 0x80;
				*(ttable) = 0;
			}
			break;
		//If character is not an ignorable white space
		} else if( *tstr != '\n' && *tstr != ' ' && *tstr != '\t' ){
			*ttable = *tstr & 0x7F;
			ttable++;
		}
		tstr++;
	}
	return entry;
}

unsigned int table_del(char* entry) {
	char* src;
	char* dst = entry;
	src = table_next(entry);
	while( 1 ) {
		*dst = *src;
		if( *src == 0 ) {
			break;
		}
		dst++;
		src++;
	}
	return src-dst;
}