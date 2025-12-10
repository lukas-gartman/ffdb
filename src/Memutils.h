#ifndef FFDB_MEMUTILS_H
#define FFDB_MEMUTILS_H

#include <cmath>

/** \brief Align `bytes` depending on amount.
 *
 *  This function aligns the given `bytes` to \ref SMALL_ALIGNMENT or \ref LARGE_ALIGNMENT depending on whether `bytes`
 *  is larger than or equal to \ref ALIGNMENT_THRESHOLD.
 *
 *  @param bytes the bytes to align
 *  @return the aligned bytes
 */
static Size align_bytes(Size bytes, Size alignment) {
    return (bytes + alignment - 1) & ~(alignment - 1);
}

/** \brief Calculate the number of bytes left logical page that has `offset`.
 *
 *  This function calculates the number of bytes left in the logical page that contains the given `offset`. It does so
 *  rather efficiently so it can be called often without performance concerns.
 *
 *  @param offset the offset from which to calculate the bytes left in the logical page
 *  @return the bytes left in the logical page
 */
static Size bytes_left_in_page(Offset offset, Size log_page_size) {
    return log_page_size - (offset & (log_page_size - 1));
}

/** \brief Find the next power of 2 that fits the sum of both arguments.
 *
 *  This function calculates the next size that is a power of 2 and fits the sum of `min_size` and `curr_size`. This is
 *  done by solving the equation `2^n >= min_size + curr_size`, where `n` is the smallest integer such that the equation
 *  holds, and then returning `2^n`.
 *
 *  @param curr_size the current size of the memory map
 *  @param min_size the minimum additional size we need
 *  @return `2^n` where `n` is the smallest integer such that `2^n >= min_size + curr_size`.
 */
static Size next_pow2_size(const Size curr_size, const Size min_size) {
    const Size n = ceil(log2((double) (min_size + curr_size)));
    return Size{1} << n;
}

static uint8_t next_padovan_tbl[] = {
    0, 1, 2, 3, 4, 5,
    7, 7,
    9, 9,
    12, 12, 12,
    16, 16, 16, 16,
    21, 21, 21, 21, 21,
    28, 28, 28, 28, 28, 28, 28,
    37, 37, 37, 37, 37, 37, 37, 37, 37
};

static Size get_next_padovan(Size min_pages) {
    if (min_pages <= 37) return next_padovan_tbl[min_pages];

    Size a = 49, b = 65, c = 86;
    while (min_pages > a) {
        if (b < a) return a; // Don't really need to worry about overflow
                             // unless our database will be larger than
                             // 16 million TB.
        Size tmp = a + b;
        a = b;
        b = c;
        c = tmp;
    }

    return a;
}

static Size next_padovan_size(Size curr_size, Size min_size, Size page_size)
{
    Size pages_needed = align_bytes(curr_size + min_size, page_size) / page_size;
    return get_next_padovan(pages_needed) * page_size;
}


#endif //FFDB_MEMUTILS_H

