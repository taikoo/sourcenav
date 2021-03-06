/*
 Copyright (c) 2000, Red Hat, Inc.

 This file is part of Source-Navigator.

 Source-Navigator is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as published
 by the Free Software Foundation; either version 2, or (at your option)
 any later version.

 Source-Navigator is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with Source-Navigator; see the file COPYING.  If not, write to
 the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
 MA 02111-1307, USA.
 */

/*
 lexinput.c

  Copyright (C) 1998 Cygnus Solutions

  Description:

  Implements a set of routines that can be used by scanners (and scanners
  generated by tools such as GNU flex) for translating character set encodings
  to ASCII using the Tcl library.
  */

#include <stdlib.h>
#include <tcl.h>

static size_t translate_crlf(char* buffer, size_t size);

static Tcl_Encoding ascii = NULL;
extern Tcl_Encoding encoding;

static start_of_file = 1;
static saved_cr = 0;
extern FILE * yyin;


int sn_encoded_input(char *buf, int max_size)
{
	char *rawbuf, *utf8buf;
	int srcRead, dstWrote, nbytes, flags = 0;

	static Tcl_EncodingState utf8_state, ascii_state;

	if(encoding == NULL) {
		/*
		 No translation necessary. Just look for CRLF
		 sequences and remove the CR character.
		 */
		size_t read = fread(buf, sizeof(char), max_size, yyin);
		read -= translate_crlf(buf, read);
		return read;
	}

	if(ascii == NULL) {
		ascii = Tcl_GetEncoding(NULL, "ascii");
		if(ascii == NULL) {
			fprintf(stderr, "Unable to locate `ascii' encoding\n");
			return 0;
		}
	}

	if(start_of_file) {
		flags |= TCL_ENCODING_START;
	}

	if((rawbuf = (char *) ckalloc(max_size)) == NULL) {
		/* Insufficient memory. */
		return 0;
	}

	/* FIXME: This ought to do it. */
	if((utf8buf = (char *) ckalloc(2 * max_size)) == NULL) {
		/* Insufficient memory. */
		return 0;
	}

	/* Read max_size bytes from disk. */
	nbytes = fread(rawbuf, sizeof(unsigned char), sizeof(rawbuf), yyin);
	if(nbytes == 0) {
		/*
		 Continue on with an empty buffer; this allows the Tcl
		 encoding routines to do any necessary finalisation.
		 See the Encoding(n) man page.
		 */
		flags = TCL_ENCODING_END;
	}

	/* Translate encoded file data into UTF-8. */
	Tcl_ExternalToUtf(NULL, encoding, rawbuf, nbytes, flags,
			  &utf8_state, utf8buf, 2 * max_size,
			  &srcRead, &dstWrote, NULL);

	/* Look for CRLF sequences and remove the CR characters */
	dstWrote -= translate_crlf(utf8buf, dstWrote);


	/*
	 FIXME This code assumes that an encoded stream `n' bytes long
	 will always reduce down to an ASCII stream no longer than `n'
	 bytes. This is a reasonable assumption, but probably not
	 foolproof.
	 */

	/* Translate this from UTF-8 to ASCII. */
	Tcl_UtfToExternal(NULL, ascii, utf8buf, dstWrote, flags,
			  &ascii_state, buf, max_size,
			  &srcRead, &dstWrote, NULL);

	if(dstWrote > 0 && start_of_file) {
		start_of_file = 0;
	}

	ckfree(utf8buf);
	ckfree(rawbuf);

	return dstWrote;
}


void sn_reset_encoding()
{
	start_of_file = 1;
	saved_cr = 0;
}


/*
 Accept a buffer that could contain CRLF sequences and convert
 the CRLF to a plain LF. Return the number of CR characters
 that were removed as a result of the translation.
 */
size_t translate_crlf(char* buffer, size_t size)
{
	int removed = 0;
	char *dtmp, *stmp;

	if(size == 0) {
		return 0;
	}

	if(saved_cr) {
		if(*buffer == '\n') {
			/* Do nothing, removes CR */
		} else {
			/*
			 A lone CR should never appear in the input.

			 Freek: Do not panic, simply remove it as with the CR.
			 This fixes potential aborts with some wrongly formatted
                         windows source files.
			 */
			/* panic("CR without trailing LF on buffer border"); */
		}
		saved_cr = 0;
	}

	for (dtmp = stmp = buffer; (stmp - buffer) < size ; stmp++, dtmp++) {
		if((*stmp == '\r') && (*(stmp + 1) == '\n')) {
			stmp++;
			removed++;
		}

		if(dtmp != stmp) {
			*dtmp = *stmp;
		}
	}

	if(*stmp == '\r') {
		saved_cr = 1;
		removed++;
	}

	return removed;
}
