#ifndef SUIT_ISOMORPHISM_H
#define SUIT_ISOMORPHISM_H

#include "hand_evaluation.h"
#include "helper.h"

// TODO: comment
// TODO: extend to turn and river

struct SuitData
{
    int numSuitPreflop {};
    int numSuitPostflop {};
    
    uint16_t suitRanksetPreflop {0};
    uint16_t suitRanksetPostflop {0};
};



std::array<SuitData, 4> suit_config(
    std::array<uint32_t, 5>& cards
)
{
    std::array<SuitData, 4> suits {};

    for (int i = 0; i < 2; ++i)
    {
        uint8_t suitMask = (cards[i] >> 13u) & 0xFu;
        for (int j = 0; j < 4; ++j)
            if ((suitMask & (1u << j)) == (1u << j))
            {
                suits[j].numSuitPreflop++;
                suits[j].suitRanksetPreflop |= (cards[i] & 0x1FFFu);
            }
    }

    for (int i = 2; i < 5; ++i)
    {
        uint8_t suitMask = (cards[i] >> 13u) & 0xFu;
        for (int j = 0; j < 4; ++j)
            if ((suitMask & (1u << j)) == (1u << j))
            {
                suits[j].numSuitPostflop++;
                suits[j].suitRanksetPostflop |= (cards[i] & 0x1FFFu);
            }
    }

    std::sort(suits.begin(), suits.end(),
    [](const SuitData& a, const SuitData& b)
    {
        if (a.numSuitPreflop != b.numSuitPreflop)
            return a.numSuitPreflop > b.numSuitPreflop; // Sort by Preflop count first
        return a.numSuitPostflop > b.numSuitPostflop;          // Sort by Flop count second
    });

    return suits;
}


int colex(
    int N,
    int M1,
    int M2
)
{
    return helper::binCoeff(N, M1) * helper::binCoeff(N-M1, M2);
}



int index_multiset (
    std::vector<int> multisetIndices
)
{
    std::sort(multisetIndices.begin(), multisetIndices.end());

    int val = 0;
    for (std::size_t i = 0; i < multisetIndices.size(); ++i)
    {
        val += helper::binCoeff(multisetIndices[i] + i, i + 1);
    }

    return val;
}



std::vector<std::array<std::array<int, 2>, 4>> list_configs()
{
    const auto min = [](int a, int b)
    {
        return a > b ? b : a;
    };

    std::vector<std::array<std::array<int, 2>, 4>> suitConfigs {};

    const auto order = [&] (std::array<int, 2>& a, std::array<int, 2>& b)
    {
        if (a[0] != b[0]) return a[0] >= b[0];
        return a[1] >= b[1];
    };

    for (int c11 = 2; c11 > -1; --c11)
        for (int c12 = min(c11, 2 - c11); c12 > -1; --c12)
            for (int c13 = min(c12, 2 - c11 - c12); c13 > -1; --c13)
            {
                int c14 = 2 - c11 - c12 - c13;
                if (c14 > c13) continue;

                for (int c21 = 3; c21 > -1; --c21)
                    for (int c22 = 3 - c21; c22 > -1; --c22)
                        for (int c23 = 3 - c21 - c22; c23 > -1; --c23)
                        {
                            int c24 = 3 - c21 - c22 - c23;

                            std::array<std::array<int, 2>, 4> config;
                            config[0] = {c11, c21};
                            config[1] = {c12, c22};
                            config[2] = {c13, c23};
                            config[3] = {c14, c24};

                            if (order(config[0], config[1]) && order(config[1], config[2]) && order(config[2], config[3]))
                                suitConfigs.push_back(config);
                        }
            }
        
    return suitConfigs;
}



std::vector<int> list_offsets
(
    std::vector<std::array<std::array<int, 2>, 4>> configsList
)
{
    std::vector<int> offsetSizes;
    int offset {0};
    offsetSizes.push_back(offset);

    for (std::array<std::array<int, 2>, 4> config: configsList)
    {
        int size {1};
        int i = 0;
        while (i < 4)
        {
            int M1 = config[i][0];
            int M2 = config[i][1];

            int diff = 0;
            int j = i;
            
            while (j < 4 && config[j][0] == M1 && config[j][1] == M2)
                j++;

            diff = j - i;
            i = j;

            size *= helper::binCoeff(colex(13, M1, M2) + diff - 1, diff);
        }
        offset += size;
        offsetSizes.push_back(offset);
    }
    
    return offsetSizes;
}



int index_rankset(
    const std::vector<int>& rankset,
    const int M
)
{
    int sum {0};
    for (int i = 1; i < M + 1; ++i)
    {
        sum += helper::binCoeff(rankset[i - 1], M - i + 1);
    }

    return sum;
}



int index_rankset_group(
    const uint16_t preflopRanks,
    const uint16_t postflopRanks
)
{
    const int M1 {__builtin_popcount(preflopRanks)};
    std::vector<int> preflopRankset {};
    preflopRankset.reserve(M1);
    const int M2 {__builtin_popcount(postflopRanks)};
    std::vector<int> postflopRankset {};
    postflopRankset.reserve(M2);

    for (int i = TWO; i <= ACE; ++i)
    {
        if ((preflopRanks & (1u << i)) == (1u << i))
        {
            preflopRankset.push_back(i);
        }
    }

    for (int i = TWO; i <= ACE; ++i)
    {
        if ((postflopRanks & (1u << i)) == (1u << i))
        {
            int shift {0};
            for (int preflopRank: preflopRankset)
            {
                if (preflopRank < i) ++shift;
            }
            postflopRankset.push_back(i - shift);
        }
    }

    std::reverse(preflopRankset.begin(), preflopRankset.end());
    std::reverse(postflopRankset.begin(), postflopRankset.end());

    return index_rankset(preflopRankset, M1) + helper::binCoeff(13, M1) * index_rankset(postflopRankset, M2);

}



int index_cards(
    std::array<uint32_t, 5>& cards
)
{
    std::array<SuitData, 4> suits {suit_config(cards)};

    std::array<int, 4> suitIndices {};

    for (int i = 0; i < 4; ++i)
    {
        suitIndices[i] = index_rankset_group(suits[i].suitRanksetPreflop, suits[i].suitRanksetPostflop);
    }

    int  i = 0;
    int localIndex = 0;
    int size = 1;
    while (i < 4)
    {
        
        int M1 {suits[i].numSuitPreflop};
        int M2 {suits[i].numSuitPostflop};
        std::vector<int> multisetIndices {};

        int j = i;
        while (j < 4 && suits[j].numSuitPreflop == M1 && suits[j].numSuitPostflop == M2)
        {
            multisetIndices.push_back(suitIndices[j]);
            ++j;
        }
        
        localIndex += size * index_multiset(multisetIndices);
        size *= helper::binCoeff(colex(13, M1, M2) + multisetIndices.size() - 1, multisetIndices.size());
        i = j;
    }

    std::vector<std::array<std::array<int, 2>, 4>> configList {list_configs()};
    std::vector<int> offsetList {list_offsets(configList)};

    std::array<std::array<int, 2>, 4> config{{
        {suits[0].numSuitPreflop, suits[0].numSuitPostflop},
        {suits[1].numSuitPreflop, suits[1].numSuitPostflop},
        {suits[2].numSuitPreflop, suits[2].numSuitPostflop},
        {suits[3].numSuitPreflop, suits[3].numSuitPostflop},
        }};

    int offset {};
    for (std::size_t i = 0; i < configList.size(); ++i)
    {
        if (config == configList[i])
        {
            offset = offsetList[i];
            break;
        }
    }

    return offset + localIndex;
}



/// UNINDEXING ///



struct localIndexInfo
{
    std::array<std::array<int, 2>, 4> suitConfig {};
    int localIndex {};
};

localIndexInfo get_local
(
    int index
)
{
    std::vector<std::array<std::array<int, 2>, 4>> listConfigs {list_configs()};
    std::vector<int> listOffsets {list_offsets(listConfigs)};
    localIndexInfo local {};

    for (std::size_t i = 1; i < listOffsets.size(); ++i)
    {
        if (index > listOffsets[i])
        {
            local.localIndex = index - listOffsets[i];
            local.suitConfig = listConfigs[i];
        }
    }

    return local;
}

struct configInfo
{
    std::vector<std::array<int, 2>>  uniqueSuitConfig;
    std::vector<int> multiplicity;
};

configInfo multiset_colex_parameters
(
    std::array<std::array<int, 2>, 4> config
)
{
    configInfo parameters {};
    parameters.uniqueSuitConfig.reserve(4);
    parameters.multiplicity.reserve(4);
    
    int i = 0;
    while(i < 4)
    {

        int  j = i;
        while(j < 4 && config[i] == config[j])
            ++j;

        if (config[i] != std::array<int, 2> {0,0})
        {
            parameters.uniqueSuitConfig.push_back(config[i]);
            parameters.multiplicity.push_back(j - i);
        }
        i = j;
    }

    return parameters;
}

int multiset_colex
(
    std::array<int, 2> C_j,
    int multiplicity
)
{
    return helper::binCoeff(colex(13, C_j[0], C_j[1]) + multiplicity - 1, multiplicity);
}

std::vector<int> get_rankset_indices
(
    int localIndex,
    configInfo parameters
)
{
    std::vector<int> ranksetIndices(parameters.uniqueSuitConfig.size());

    int index = localIndex;
    for (int i = parameters.uniqueSuitConfig.size(); i > 0; --i)
    {
        int size = 1;
        for (int j = 0; j < i; ++j)
        {
            size *= multiset_colex(parameters.uniqueSuitConfig[j], parameters.multiplicity[j]);
        }
        ranksetIndices[i] = index / size;
        index = index % size;
    }

    ranksetIndices[0] = index;
    return ranksetIndices;
}

std::vector<int> get_suited_indices
(
    int ranksetIndex,
    int multiplicity
)
{
    std::vector<int> indices (multiplicity);

    if (multiplicity == 1)
    {
        indices[0] = ranksetIndex;
        return indices;
    }

    int index = ranksetIndex;
    for (int i = multiplicity - 1; i > 0; --i)
    {
        int j = 0;
        while(helper::binCoeff(j + i, i + 1) <= index) ++j;
        indices[i] = j - 1;
        index = index % helper::binCoeff(j - 1 + i, i + 1);
    }
    indices[0] = index;

    return indices;
}

std::array<std::vector<int>, 2> get_suited_card_ranks
(
    int suitIndex,
    std::array<int, 2> suitConfig
)
{
    int index {suitIndex};
    int cardIndexPostflop = {index / helper::binCoeff(13, suitConfig[0])};
    int numCardsPostflop = suitConfig[1];
    int cardIndexPreflop = {index % helper::binCoeff(13, suitConfig[0])};
    int numCardsPreflop = suitConfig[0];

    const auto& highest_binCoeff = [] (int numCards, int index) // 2, 66
    {
        std::vector<int> cards;
        
        for (int i = 0; i < numCards; ++i)
        {
            int j = 0;
            while (helper::binCoeff(j, numCards - i) <= index) ++j;
            cards.push_back(j-1);

            int val {helper::binCoeff(j - 1, numCards - i)};
            val > 0 ? index %= val : index = 0;
        }

        return cards;
    };

    std::vector<int> cardsPreflop = highest_binCoeff(numCardsPreflop, cardIndexPreflop);
    std::vector<int> cardsPostflop = highest_binCoeff(numCardsPostflop, cardIndexPostflop);

    for (int i = 0; i < static_cast<int>(cardsPostflop.size()); ++i)
        for (int j = static_cast<int>(cardsPreflop.size()) - 1; j > -1 ; --j)
        {
            if (cardsPreflop[j] <= cardsPostflop[i]) ++cardsPostflop[i];
        }

    std::array<std::vector<int>, 2> finalCards {cardsPreflop, cardsPostflop};

    return finalCards;
}

struct SuitGroup
{
    int preflopCards;
    int postflopCards;
    int Suit;
};

std::array<uint32_t, 5> unindex
(
    int index
)
{
    localIndexInfo local {get_local(index)};

    configInfo parameters {multiset_colex_parameters(local.suitConfig)};
    std::vector<std::array<int, 2>> config {parameters.uniqueSuitConfig};
    std::vector<int> multiplicity {parameters.multiplicity};

    std::vector<int> rankset_indices {get_rankset_indices(local.localIndex, parameters)};

    std::vector<int> suitIndex {};
    suitIndex.reserve(4);
    for (std::size_t i = 0; i < multiplicity.size(); ++i)
    {
        std::vector<int> indices {get_suited_indices(rankset_indices[i], multiplicity[i])};
        std::reverse(indices.begin(), indices.end());
        for (int index: indices)
        {
            suitIndex.push_back(index);
        }
    }

    std::vector<uint32_t> preflopBitwiseCards;
    std::vector<uint32_t> postflopBitwiseCards;
    for (std::size_t i = 0; i < 4; ++i)
    {
        std::array<int, 2> C {local.suitConfig[i]};
        if (C == std::array<int, 2> {0,0}) continue;

        std::array<std::vector<int>, 2> groupedCards {get_suited_card_ranks(suitIndex[i], C)};

        std::vector<int> preflopCards = groupedCards[0];
        std::vector<int> postflopCards = groupedCards[1];

        for (std::size_t j = 0; j < preflopCards.size(); ++j)
        {
            preflopBitwiseCards.push_back((1u << (i + 13u)) | (1u << preflopCards[j]));
        }
        for (std::size_t j = 0; j < postflopCards.size(); ++j)
        {
            postflopBitwiseCards.push_back((1u << (i + 13u)) | (1u << postflopCards[j]));
        }

    }

    std::array<uint32_t, 5> cards {};
    for (int i = 0; i < 2; ++i)
    {
        cards[i] = preflopBitwiseCards[i];
    }
    for (int i = 0; i < 3; ++i)
    {
        cards[i + 2] = postflopBitwiseCards[i];
    }

    return cards;

}

#endif