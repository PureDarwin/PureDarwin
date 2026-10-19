/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _FBSD_COMPAT_SYS_RMAN_H_
#define _FBSD_COMPAT_SYS_RMAN_H_


#include <machine/_bus.h>
#include <machine/resource.h>


#define RF_ACTIVE		0x0002
#define RF_SHAREABLE	0x0004
#define RF_OPTIONAL		0x0080

#define RF_ALIGNMENT_SHIFT		10 /* alignment size bit starts bit 10 */
#define RF_ALIGNMENT_LOG2(x)	((x) << RF_ALIGNMENT_SHIFT)

struct resource {
	int					r_type;
	bus_space_tag_t		r_bustag;		/* bus_space tag */
	bus_space_handle_t	r_bushandle;	/* bus_space handle */
	area_id				r_mapped_area;
};


bus_space_handle_t rman_get_bushandle(struct resource *);
bus_space_tag_t rman_get_bustag(struct resource *);
int rman_get_rid(struct resource *);
void* rman_get_virtual(struct resource *);
bus_addr_t rman_get_start(struct resource *);
bus_size_t rman_get_size(struct resource *);

#endif /* _FBSD_COMPAT_SYS_RMAN_H_ */
