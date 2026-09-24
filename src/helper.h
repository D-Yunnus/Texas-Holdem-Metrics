#ifndef HELPER_H
#define HELPER_H

#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <bit>
#include <utility>
#include <chrono>
#include <omp.h>


namespace helper
{
    // Prints bitwise (code) cards into standard english cards
    inline void printCard(
        const uint32_t Card
    )
    {
        const uint32_t rank {Card & 0x1FFF};
        const uint32_t suit {Card & 0x1E000};

        switch (rank)
        {
        case 0x1:
            std::cout << "TWO OF ";
            break;
        case 0x2:
            std::cout << "THREE OF ";
            break;
        case 0x4:
            std::cout << "FOUR OF ";
            break;
        case 0x8:
            std::cout << "FIVE OF ";
            break;
        case 0x10:
            std::cout << "SIX OF ";
            break;
        case 0x20:
            std::cout << "SEVEN OF ";
            break;
        case 0x40:
            std::cout << "EIGHT OF ";
            break;
        case 0x80:
            std::cout << "NINE OF ";
            break;
        case 0x100:
            std::cout << "TEN OF ";
            break;
        case 0x200:
            std::cout << "JACK OF ";
            break;
        case 0x400:
            std::cout << "QUEEN OF ";
            break;
        case 0x800:
            std::cout << "KING OF ";
            break;
        case 0x1000:
            std::cout << "ACE OF ";
            break;
        }

        switch (suit)
        {
        case 0x2000:
            std::cout << "SPADES\n";
            break;
        case 0x4000:
            std::cout << "HEARTS\n";
            break;
        case 0x8000:
            std::cout << "DIAMONDS\n";
            break;
        case 0x10000:
            std::cout << "CLUBS\n";
            break;
        }
    }

    // helper function to concatenate two arrays
    template <typename T, std::size_t A, std::size_t B>
    std::array<T, A + B> concatenate(
        const std::array<T, A>& first,
        const std::array<T, B>& second
    )
    {
        std::array<T, A + B> result;
        std::copy(first.begin(), first.end(), result.begin());
        std::copy(second.begin(), second.end(), result.begin() + A);

        return result;
    }

    constexpr int binCoeff(int N, int K)
    {
        if (K > N) return 0;
        if (K == 0 || K == N) return 1;

        int k = (K > N / 2) ? (N - K) : K;

        int res = 1;
        for (int i = 1; i <= k; ++i)
        {
            res = res * (N - k + i) / i;
        }

        return res;
    }
}

#endif