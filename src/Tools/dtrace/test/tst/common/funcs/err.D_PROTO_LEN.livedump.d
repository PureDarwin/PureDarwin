/*
 * Copyright 2021 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * ASSERTION:
 *    Verify that livedump() handles too few arguments passed.
 *
 * SECTION: Actions and Subroutines/livedump()
 *
 */
BEGIN
{
    livedump();
    exit(0);
}
