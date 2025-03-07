// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only


/*
These functions are based on:

-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.

These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
are externally useful functions.  Routines to test the hash are included
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
-------------------------------------------------------------------------------
*/

#include <QtGlobal>

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#endif

#define hashsize(n) ((quint32)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
--------------------------------------------------------------------
 This works on all machines.  To be useful, it requires
 -- that the key be an array of quint32's, and
 -- that the length be the number of quint32's in the key

 The function hashword() is identical to hashlittle() on little-endian
 machines, and identical to hashbig() on big-endian machines,
 except that the length has to be measured in quint32s rather than in
 bytes.  hashlittle() is more complicated than hashword() only because
 hashlittle() has to dance around fitting the key bytes into registers.
--------------------------------------------------------------------
*/
quint32 hashword(
const quint32 *k,                   /* the key, an array of quint32 values */
size_t          length,               /* the length of the key, in quint32s */
quint32        initval)         /* the previous hash, or an arbitrary value */
{
  quint32 a,b,c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + (((quint32)length)<<2) + initval;

  /*------------------------------------------------- handle most of the key */
  while (length > 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a,b,c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 quint32's */
  switch (length)                     /* all the case statements fall through */
  {
  case 3 : c+=k[2];
           Q_FALLTHROUGH();
  case 2 : b+=k[1];
           Q_FALLTHROUGH();
  case 1 : a+=k[0];
           final(a,b,c);
           Q_FALLTHROUGH();
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  return c;
}


/*
--------------------------------------------------------------------
hashword2() -- same as hashword(), but take two seeds and return two
32-bit values.  pc and pb must both be nonnull, and *pc and *pb must
both be initialized with seeds.  If you pass in (*pb)==0, the output
(*pc) will be the same as the return value from hashword().
--------------------------------------------------------------------
*/
void hashword2 (
const quint32 *k,                   /* the key, an array of quint32 values */
size_t          length,               /* the length of the key, in quint32s */
quint32       *pc,                      /* IN: seed OUT: primary hash value */
quint32       *pb)               /* IN: more seed OUT: secondary hash value */
{
  quint32 a,b,c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((quint32)(length<<2)) + *pc;
  c += *pb;

  /*------------------------------------------------- handle most of the key */
  while (length > 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a,b,c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 quint32's */
  switch (length)                     /* all the case statements fall through */
  {
  case 3 : c+=k[2];
           Q_FALLTHROUGH();
  case 2 : b+=k[1];
           Q_FALLTHROUGH();
  case 1 : a+=k[0];
    final(a,b,c);
           Q_FALLTHROUGH();
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  *pc=c; *pb=b;
}


/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (quint8 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/

quint32 hashlittle( const void *key, size_t length, quint32 initval)
{
  quint32 a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((quint32)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const quint32 *k = (const quint32 *)key;         /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticeably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch (length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    const quint8  *k8 = (const quint8 *)k;
    switch (length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((quint32)k8[10])<<16;
             Q_FALLTHROUGH();
    case 10: c+=((quint32)k8[9])<<8;
             Q_FALLTHROUGH();
    case 9 : c+=k8[8];
             Q_FALLTHROUGH();
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((quint32)k8[6])<<16;
             Q_FALLTHROUGH();
    case 6 : b+=((quint32)k8[5])<<8;
             Q_FALLTHROUGH();
    case 5 : b+=k8[4];
             Q_FALLTHROUGH();
    case 4 : a+=k[0]; break;
    case 3 : a+=((quint32)k8[2])<<16;
             Q_FALLTHROUGH();
    case 2 : a+=((quint32)k8[1])<<8;
             Q_FALLTHROUGH();
    case 1 : a+=k8[0]; break;
    case 0 : return c;
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const quint16 *k = (const quint16 *)key;         /* read 16-bit chunks */
    const quint8  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((quint32)k[1])<<16);
      b += k[2] + (((quint32)k[3])<<16);
      c += k[4] + (((quint32)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const quint8 *)k;
    switch (length)
    {
    case 12: c+=k[4]+(((quint32)k[5])<<16);
             b+=k[2]+(((quint32)k[3])<<16);
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 11: c+=((quint32)k8[10])<<16;
             Q_FALLTHROUGH();
    case 10: c+=k[4];
             b+=k[2]+(((quint32)k[3])<<16);
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 9 : c+=k8[8];
             Q_FALLTHROUGH();
    case 8 : b+=k[2]+(((quint32)k[3])<<16);
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 7 : b+=((quint32)k8[6])<<16;
             Q_FALLTHROUGH();
    case 6 : b+=k[2];
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 5 : b+=k8[4];
             Q_FALLTHROUGH();
    case 4 : a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 3 : a+=((quint32)k8[2])<<16;
             Q_FALLTHROUGH();
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const quint8 *k = (const quint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((quint32)k[1])<<8;
      a += ((quint32)k[2])<<16;
      a += ((quint32)k[3])<<24;
      b += k[4];
      b += ((quint32)k[5])<<8;
      b += ((quint32)k[6])<<16;
      b += ((quint32)k[7])<<24;
      c += k[8];
      c += ((quint32)k[9])<<8;
      c += ((quint32)k[10])<<16;
      c += ((quint32)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length)                   /* all the case statements fall through */
    {
    case 12: c+=((quint32)k[11])<<24;
             Q_FALLTHROUGH();
    case 11: c+=((quint32)k[10])<<16;
             Q_FALLTHROUGH();
    case 10: c+=((quint32)k[9])<<8;
             Q_FALLTHROUGH();
    case 9 : c+=k[8];
             Q_FALLTHROUGH();
    case 8 : b+=((quint32)k[7])<<24;
             Q_FALLTHROUGH();
    case 7 : b+=((quint32)k[6])<<16;
             Q_FALLTHROUGH();
    case 6 : b+=((quint32)k[5])<<8;
             Q_FALLTHROUGH();
    case 5 : b+=k[4];
             Q_FALLTHROUGH();
    case 4 : a+=((quint32)k[3])<<24;
             Q_FALLTHROUGH();
    case 3 : a+=((quint32)k[2])<<16;
             Q_FALLTHROUGH();
    case 2 : a+=((quint32)k[1])<<8;
             Q_FALLTHROUGH();
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}


/*
 * hashlittle2: return 2 32-bit hash values
 *
 * This is identical to hashlittle(), except it returns two 32-bit hash
 * values instead of just one.  This is good enough for hash table
 * lookup with 2^^64 buckets, or if you want a second hash if you're not
 * happy with the first, or if you want a probably-unique 64-bit ID for
 * the key.  *pc is better mixed than *pb, so use *pc first.  If you want
 * a 64-bit value do something like "*pc + (((uint64_t)*pb)<<32)".
 */
void hashlittle2(
  const void *key,       /* the key to hash */
  size_t      length,    /* length of the key */
  quint32   *pc,        /* IN: primary initval, OUT: primary hash */
  quint32   *pb)        /* IN: secondary initval, OUT: secondary hash */
{
  quint32 a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((quint32)length) + *pc;
  c += *pb;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const quint32 *k = (const quint32 *)key;         /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticeably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch (length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    const quint8  *k8 = (const quint8 *)k;
    switch (length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((quint32)k8[10])<<16;
             Q_FALLTHROUGH();
    case 10: c+=((quint32)k8[9])<<8;
             Q_FALLTHROUGH();
    case 9 : c+=k8[8];
             Q_FALLTHROUGH();
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((quint32)k8[6])<<16;
             Q_FALLTHROUGH();
    case 6 : b+=((quint32)k8[5])<<8;
             Q_FALLTHROUGH();
    case 5 : b+=k8[4];
             Q_FALLTHROUGH();
    case 4 : a+=k[0]; break;
    case 3 : a+=((quint32)k8[2])<<16;
             Q_FALLTHROUGH();
    case 2 : a+=((quint32)k8[1])<<8;
             Q_FALLTHROUGH();
    case 1 : a+=k8[0]; break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const quint16 *k = (const quint16 *)key;         /* read 16-bit chunks */
    const quint8  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((quint32)k[1])<<16);
      b += k[2] + (((quint32)k[3])<<16);
      c += k[4] + (((quint32)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const quint8 *)k;
    switch (length)
    {
    case 12: c+=k[4]+(((quint32)k[5])<<16);
             b+=k[2]+(((quint32)k[3])<<16);
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 11: c+=((quint32)k8[10])<<16;
             Q_FALLTHROUGH();
    case 10: c+=k[4];
             b+=k[2]+(((quint32)k[3])<<16);
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 9 : c+=k8[8];
             Q_FALLTHROUGH();
    case 8 : b+=k[2]+(((quint32)k[3])<<16);
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 7 : b+=((quint32)k8[6])<<16;
             Q_FALLTHROUGH();
    case 6 : b+=k[2];
             a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 5 : b+=k8[4];
             Q_FALLTHROUGH();
    case 4 : a+=k[0]+(((quint32)k[1])<<16);
             break;
    case 3 : a+=((quint32)k8[2])<<16;
             Q_FALLTHROUGH();
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const quint8 *k = (const quint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((quint32)k[1])<<8;
      a += ((quint32)k[2])<<16;
      a += ((quint32)k[3])<<24;
      b += k[4];
      b += ((quint32)k[5])<<8;
      b += ((quint32)k[6])<<16;
      b += ((quint32)k[7])<<24;
      c += k[8];
      c += ((quint32)k[9])<<8;
      c += ((quint32)k[10])<<16;
      c += ((quint32)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length)                   /* all the case statements fall through */
    {
    case 12: c+=((quint32)k[11])<<24;
             Q_FALLTHROUGH();
    case 11: c+=((quint32)k[10])<<16;
             Q_FALLTHROUGH();
    case 10: c+=((quint32)k[9])<<8;
             Q_FALLTHROUGH();
    case 9 : c+=k[8];
             Q_FALLTHROUGH();
    case 8 : b+=((quint32)k[7])<<24;
             Q_FALLTHROUGH();
    case 7 : b+=((quint32)k[6])<<16;
             Q_FALLTHROUGH();
    case 6 : b+=((quint32)k[5])<<8;
             Q_FALLTHROUGH();
    case 5 : b+=k[4];
             Q_FALLTHROUGH();
    case 4 : a+=((quint32)k[3])<<24;
             Q_FALLTHROUGH();
    case 3 : a+=((quint32)k[2])<<16;
             Q_FALLTHROUGH();
    case 2 : a+=((quint32)k[1])<<8;
             Q_FALLTHROUGH();
    case 1 : a+=k[0];
             break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }
  }

  final(a,b,c);
  *pc=c; *pb=b;
}



/*
 * hashbig():
 * This is the same as hashword() on big-endian machines.  It is different
 * from hashlittle() on all machines.  hashbig() takes advantage of
 * big-endian byte ordering.
 */
quint32 hashbig( const void *key, size_t length, quint32 initval)
{
  quint32 a,b,c;
  union { const void *ptr; size_t i; } u; /* to cast key to (size_t) happily */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((quint32)length) + initval;

  u.ptr = key;
  if (HASH_BIG_ENDIAN && ((u.i & 0x3) == 0)) {
    const quint32 *k = (const quint32 *)key;         /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]<<8" actually reads beyond the end of the string, but
     * then shifts out the part it's not allowed to read.  Because the
     * string is aligned, the illegal read is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticeably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch (length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff00; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff0000; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff000000; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff00; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff0000; a+=k[0]; break;
    case 5 : b+=k[1]&0xff000000; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff00; break;
    case 2 : a+=k[0]&0xffff0000; break;
    case 1 : a+=k[0]&0xff000000; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }

#else  /* make valgrind happy */

    const quint8 *k8 = (const quint8 *)k;
    switch (length)                   /* all the case statements fall through */
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((quint32)k8[10])<<8;
             Q_FALLTHROUGH();
    case 10: c+=((quint32)k8[9])<<16;
             Q_FALLTHROUGH();
    case 9 : c+=((quint32)k8[8])<<24;
             Q_FALLTHROUGH();
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((quint32)k8[6])<<8;
             Q_FALLTHROUGH();
    case 6 : b+=((quint32)k8[5])<<16;
             Q_FALLTHROUGH();
    case 5 : b+=((quint32)k8[4])<<24;
             Q_FALLTHROUGH();
    case 4 : a+=k[0]; break;
    case 3 : a+=((quint32)k8[2])<<8;
             Q_FALLTHROUGH();
    case 2 : a+=((quint32)k8[1])<<16;
             Q_FALLTHROUGH();
    case 1 : a+=((quint32)k8[0])<<24; break;
    case 0 : return c;
    }

#endif /* !VALGRIND */

  } else {                        /* need to read the key one byte at a time */
    const quint8 *k = (const quint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += ((quint32)k[0])<<24;
      a += ((quint32)k[1])<<16;
      a += ((quint32)k[2])<<8;
      a += ((quint32)k[3]);
      b += ((quint32)k[4])<<24;
      b += ((quint32)k[5])<<16;
      b += ((quint32)k[6])<<8;
      b += ((quint32)k[7]);
      c += ((quint32)k[8])<<24;
      c += ((quint32)k[9])<<16;
      c += ((quint32)k[10])<<8;
      c += ((quint32)k[11]);
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length)                   /* all the case statements fall through */
    {
    case 12: c+=k[11];
             Q_FALLTHROUGH();
    case 11: c+=((quint32)k[10])<<8;
             Q_FALLTHROUGH();
    case 10: c+=((quint32)k[9])<<16;
             Q_FALLTHROUGH();
    case 9 : c+=((quint32)k[8])<<24;
             Q_FALLTHROUGH();
    case 8 : b+=k[7];
             Q_FALLTHROUGH();
    case 7 : b+=((quint32)k[6])<<8;
             Q_FALLTHROUGH();
    case 6 : b+=((quint32)k[5])<<16;
             Q_FALLTHROUGH();
    case 5 : b+=((quint32)k[4])<<24;
             Q_FALLTHROUGH();
    case 4 : a+=k[3];
             Q_FALLTHROUGH();
    case 3 : a+=((quint32)k[2])<<8;
             Q_FALLTHROUGH();
    case 2 : a+=((quint32)k[1])<<16;
             Q_FALLTHROUGH();
    case 1 : a+=((quint32)k[0])<<24;
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}
