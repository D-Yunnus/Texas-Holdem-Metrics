#ifndef CARD_EVALUATION_H
#define CARD_EVALUATION_H

#include "helper.h"

enum PokerHands : uint64_t
{
    HIGH_CARD = 0,
    PAIR,
    TWO_PAIR,
    THREE_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_KIND,
    STRAIGHT_FLUSH,
    ROYAL_FLUSH,
};

enum Ranks : uint16_t
{
    TWO = 0,
    THREE,
    FOUR,
    FIVE,
    SIX,
    SEVEN,
    EIGHT,
    NINE,
    TEN,
    JACK,
    QUEEN,
    KING,
    ACE
};

enum Suits : uint16_t
{
    SPADES = 0,
    CLUBS,
    DIAMONDS,
    HEARTS,
};

struct HandInfo
{
    PokerHands pokerRank {0};

    int straightHigh {-1};
    int lowPair {0};
    int highPair {0};
    int trip {0};
    int quad {0};
    std::array<int,5> kickers {0};
};

enum winStatus: int
{
    LOSS = 0,
    WIN,
    DRAW,
};

// converts card described by enums (Ranks, Suits) into a sequence of bits to do bitwise operations
inline constexpr uint32_t bitwise_card(
    const Ranks rank,
    const Suits suit
)
{
    return (1u << static_cast<uint16_t>(rank)) | (1u << (static_cast<uint16_t>(suit) + 13u));
}

// Takes in an array of cards and finds the score of the best hand
// TODO: use this function to generate a lookup table of data for faster evaluation
template <const std::size_t numCards> // takes in any number of cards (TODO: assert numCards >=5)
inline HandInfo determine_score(
    const std::array<uint32_t, numCards>& cards
)
{
    HandInfo info {};
    std::array<int, 13> freq {};
    std::array<uint32_t, 4> suitRanks {};
    uint32_t rankMask {0};

    for (const uint32_t card : cards)
    {
        const uint32_t cardRanks {card & 0x1FFFu};
        const int rank {__builtin_ctz(cardRanks)}; // find rank...
        const int suit {__builtin_ctz(card & 0x1E000u) - 13}; // ...and suit

        ++freq[rank]; // how often a rank appears in the set of cards
        rankMask |= cardRanks; // a mask of all ranks present
        suitRanks[suit] |= cardRanks; // a mask of all the ranks of a given suit
    }

    // lambda function to find the highest value of a straight
    const auto straightHigh = [&](const uint32_t ranks)
    {
        for (int high {ACE}; high >= SIX; --high)
        {
            const uint32_t straightMask {0x1Fu << (high - 4)};
            if ((ranks & straightMask) == straightMask)
                return high;
        }

        if ((ranks & 0x100Fu) == 0x100Fu)
            return static_cast<int>(FIVE);

        return -1; // if no straight is present then -1
    };

    // the number of ranks of each suit
    // if there is a suit with five different ranks then flush is present
    int flushSuit {-1};
    for (int suit {0}; suit < 4; ++suit)
    {
        if (__builtin_popcount(suitRanks[suit]) >= 5)
        {
            flushSuit = suit;
            break;
        }
    }

    // if flush
    if (flushSuit != -1)
    {
        const int flushStraightHigh {straightHigh(suitRanks[flushSuit])}; // check if the flush suit is also a straight
        if (flushStraightHigh != -1) // if its a straight flush
        {
            info.straightHigh = flushStraightHigh;
            info.pokerRank = (flushStraightHigh == ACE) ? ROYAL_FLUSH : STRAIGHT_FLUSH; // check if the highest rank of the straight is ACE
            return info;
        }

        info.pokerRank = FLUSH; // flush if the flush suit is not a straight

        // determine kicker cards (incase of draw)
        int kickerCount {0};
        for (int rank {ACE}; rank >= TWO && kickerCount < 5; --rank)
        {
            if ((suitRanks[flushSuit] & (1u << rank)) != 0)
                info.kickers[kickerCount++] = rank;
        }
        return info;
    }

    info.straightHigh = straightHigh(rankMask);
    if (info.straightHigh != -1) // if straight
    {
        info.pokerRank = STRAIGHT;
        return info;
    }

    // initialise counting for trips and pairs
    // for six or more cards need to consider the case of two trips present (highTrip/lowTrip)
    int tripsCount {0};
    int highTrip {-1};
    int lowTrip {-1};
    int pairCount {0};

    for (int rank {ACE}; rank >= TWO; --rank)
    {
        if (freq[rank] == 4) // if quad
        {
            info.pokerRank = FOUR_KIND;
            info.quad = rank;

            // determine kickers
            for (int kicker {ACE}; kicker >= TWO; --kicker)
            {
                if (kicker != rank && freq[kicker] > 0)
                {
                    info.kickers[0] = kicker;
                    return info;
                }
            }
        }
        else if (freq[rank] == 3) // if trips
        {
            if (tripsCount++ == 0)  // if its the first trip recorded assign high trip...
                highTrip = rank;
            else                    // ...else its the low trip
                lowTrip = rank; 
        }
        else if (freq[rank] == 2) // if pairs
        {
            if (pairCount++ == 0)           // if its the first pair recorded assign high pair...
                info.highPair = rank;
            else if (info.lowPair == 0)     // ...else its the low pair
                info.lowPair = rank;// 
        }
    }

    if (tripsCount > 0 && (pairCount > 0 || lowTrip != -1)) // if there are more than two trips or a trip and pair
    {
        info.pokerRank = FULL_HOUSE;
        info.trip = highTrip;
        info.highPair = lowTrip != -1 ? lowTrip : info.highPair;
        return info;
    }

    if (tripsCount > 0) // if there is a trip
    {
        info.pokerRank = THREE_KIND;
        info.trip = highTrip;
        int kickerCount {0};

        // determine kickers
        for (int rank {ACE}; rank >= TWO && kickerCount < 2; --rank)
        {
            if (rank != highTrip && freq[rank] > 0)
                info.kickers[kickerCount++] = rank;
        }
        return info;
    }

    if (pairCount >= 2) // if there is two pairs
    {
        info.pokerRank = TWO_PAIR;

        // determine kicker
        int kickerCount {0};
        for (int rank {ACE}; rank >= TWO; --rank)
        {
            if (freq[rank] == 1 && kickerCount == 0)
                info.kickers[kickerCount++] = rank;
        }
        return info;
    }

    if (pairCount == 1) // if there is only one pair
    {
        info.pokerRank = PAIR;
        int kickerCount {0};

        // determine kickers
        for (int rank {ACE}; rank >= TWO && kickerCount < 3; --rank)
        {
            if (freq[rank] == 1)
                info.kickers[kickerCount++] = rank;
        }
        return info;
    }

    // if its none of the other poker ranks it is a high card
    info.pokerRank = HIGH_CARD;

    // determine kickers
    for (int rank {ACE}, kickerCount {0}; rank >= TWO && kickerCount < 5; --rank)
    {
        if (freq[rank] > 0)
            info.kickers[kickerCount++] = rank;
    }
    return info; // encode all scoring data in HandInfo struct
}



// Given two players best hands determine whether the Hero (p1) wins showdown
inline winStatus compare_hands(
    const HandInfo p1Info, const HandInfo p2Info
)
{
    // highest rank wins
    if (p1Info.pokerRank != p2Info.pokerRank)
        return static_cast<winStatus>(p1Info.pokerRank > p2Info.pokerRank);

    // in cases of the hero and villain having the same rank
    switch (p1Info.pokerRank)
    {
    case STRAIGHT_FLUSH: // compare the straight's highest card
    {
        if (p1Info.straightHigh != p2Info.straightHigh)
            return static_cast<winStatus>(p1Info.straightHigh > p2Info.straightHigh);
        break;
    }
    case FOUR_KIND: // compare the quads first then kickers
    {
        if (p1Info.quad != p2Info.quad)
            return static_cast<winStatus>(p1Info.quad > p2Info.quad);
        else if (p1Info.kickers[0] != p2Info.kickers[0])
            return static_cast<winStatus>(p1Info.kickers[0] > p2Info.kickers[0]);
        break;
    }
    case FULL_HOUSE: // compare the trip first then pair
    {
        if (p1Info.trip != p2Info.trip)
            return static_cast<winStatus>(p1Info.trip > p2Info.trip);
        else if (p1Info.highPair != p2Info.highPair)
            return static_cast<winStatus>(p1Info.highPair > p2Info.highPair);
        break;
    }
    case FLUSH: // compare the kickers
    {
        int count {0};
        for (int i {0}; i < 5; i++)
        {
            if (p1Info.kickers[count] != p2Info.kickers[count])
                return static_cast<winStatus>(p1Info.kickers[count] > p2Info.kickers[count]);
            ++count;
        }
        break;
    }
    case STRAIGHT: // same as straight flush
    {
        if (p1Info.straightHigh != p2Info.straightHigh)
            return static_cast<winStatus>(p1Info.straightHigh > p2Info.straightHigh);
        break;
    }
    case THREE_KIND: // compare trip first then kickers
    {
        if (p1Info.trip != p2Info.trip)
            return static_cast<winStatus>(p1Info.trip > p2Info.trip);
        else if (p1Info.kickers[0] != p2Info.kickers[0])
            return static_cast<winStatus>(p1Info.kickers[0] > p2Info.kickers[0]);
        else if (p1Info.kickers[1] != p2Info.kickers[1])
            return static_cast<winStatus>(p1Info.kickers[1] > p2Info.kickers[1]);
        break;
    }
    case TWO_PAIR: // compare the high pair first then the low pair then the kicker
    {
        if (p1Info.highPair != p2Info.highPair)
            return static_cast<winStatus>(p1Info.highPair > p2Info.highPair);
        else if (p1Info.lowPair != p2Info.lowPair)
            return static_cast<winStatus>(p1Info.lowPair > p2Info.lowPair);
        else if (p1Info.kickers[0] != p2Info.kickers[0])
            return static_cast<winStatus>(p1Info.kickers[0] > p2Info.kickers[0]);
        break;
    }
    case PAIR: // compare pair first then kickers
    {
        if (p1Info.highPair != p2Info.highPair)
            return static_cast<winStatus>(p1Info.highPair > p2Info.highPair);

        int count {0};
        for (int i {0}; i < 3; i++)
        {
            if (p1Info.kickers[count] != p2Info.kickers[count])
                return static_cast<winStatus>(p1Info.kickers[count] > p2Info.kickers[count]);
            ++count;
        }
        break;
    }
    case HIGH_CARD: // same as flush
    {
        int count {0};
        for (int i {0}; i < 5; i++)
        {
            if (p1Info.kickers[count] != p2Info.kickers[count])
                return static_cast<winStatus>(p1Info.kickers[count] > p2Info.kickers[count]);
            ++count;
        }
        break;
    }
    default:
        break;
    }
    return DRAW; // if no one can be a definitive victor the showdown is a draw
}

#endif