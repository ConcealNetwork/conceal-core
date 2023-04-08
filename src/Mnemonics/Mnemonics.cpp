// Copyright 2014-2018 The Monero Developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Please see the included LICENSE file for more information.

#include <algorithm>

#include <Mnemonics/CRC32.h>
#include <Mnemonics/Mnemonics.h>
#include <Mnemonics/WordList.h>

#include <sstream>

namespace mnemonics
{
    crypto::SecretKey mnemonicToPrivateKey(const std::string &words)
    {
        std::vector<std::string> wordsList;

        std::istringstream stream(words);

        /* Convert whitespace separated string into vector of words */
        for (std::string word; stream >> word;)
        {
            wordsList.push_back(word);
        }

        return mnemonicToPrivateKey(wordsList);
    }

    /* Note - if the returned string is not empty, it is an error message, and
       the returned secret key is not initialized. */
    crypto::SecretKey mnemonicToPrivateKey(const std::vector<std::string> &words)
    {
        const size_t len = words.size();

        /* Mnemonics must be 25 words long */
        if (len != 25)
        {
            /* Write out "word" or "words" to make the grammar of the next sentence
               correct, based on if we have 1 or more words */
            const std::string wordPlural = len == 1 ? "word" : "words";

            return crypto::SecretKey();
        }

        /* All words must be present in the word list */
        for (auto word : words)
        {
            /* Convert to lower case */
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);

            if (std::find(WordList::English.begin(),
                          WordList::English.end(), word) == WordList::English.end())
            {
                return crypto::SecretKey();
            }
        }

        /* The checksum must be correct */
        if (!hasValidChecksum(words))
        {
            return crypto::SecretKey();
        }

        auto wordIndexes = getWordIndexes(words);

        std::vector<uint8_t> data;

        for (size_t i = 0; i < words.size() - 1; i += 3)
        {
            /* Take the indexes of these three words in the word list */
            const uint32_t w1 = wordIndexes[i];
            const uint32_t w2 = wordIndexes[i + 1];
            const uint32_t w3 = wordIndexes[i + 2];

            /* Word list length */
            const size_t wlLen = WordList::English.size();

            /* no idea what this does lol */
            const auto val = static_cast<uint32_t>(
                w1 + wlLen * (((wlLen - w1) + w2) % wlLen) + wlLen * wlLen * (((wlLen - w2) + w3) % wlLen));

            /* Don't know what this is testing either */
            if (!(val % wlLen == w1))
            {
                return crypto::SecretKey();
            }

            /* Interpret val as 4 uint8_t's */
            const auto ptr = reinterpret_cast<const uint8_t *>(&val);

            /* Append to private key */
            for (int j = 0; j < 4; j++)
            {
                data.push_back(ptr[j]);
            }
        }

        auto key = crypto::SecretKey();

        /* Copy the data to the secret key */
        std::copy(data.begin(), data.end(), key.data);

        return key;
    }

    std::string privateKeyToMnemonic(const crypto::SecretKey &privateKey)
    {
        std::vector<std::string> words;

        for (int i = 0; i < 32 - 1; i += 4)
        {
            /* Read the array as a uint32_t array */
            auto ptr = (uint32_t *)&privateKey.data[i];

            /* Take the first element of the array (since we have already 
               done the offset */
            const uint32_t val = ptr[0];

            size_t wlLen = WordList::English.size();

            const uint32_t w1 = val % wlLen;
            const uint32_t w2 = ((val / wlLen) + w1) % wlLen;
            const uint32_t w3 = (((val / wlLen) / wlLen) + w2) % wlLen;

            words.emplace_back(WordList::English[w1]);
            words.emplace_back(WordList::English[w2]);
            words.emplace_back(WordList::English[w3]);
        }

        words.push_back(getChecksumWord(words));

        std::string result;

        for (auto it = words.begin(); it != words.end(); it++)
        {
            if (it != words.begin())
            {
                result += " ";
            }

            result += *it;
        }

        return result;
    }

    /* Assumes the input is 25 words long */
    bool hasValidChecksum(const std::vector<std::string> &words)
    {
        /* Make a copy since erase() is mutating */
        auto wordsNoChecksum = words;

        /* Remove the last checksum word */
        wordsNoChecksum.erase(wordsNoChecksum.end() - 1);

        /* Assert the last word (the checksum word) is equal to the derived
           checksum */
        return words[words.size() - 1] == getChecksumWord(wordsNoChecksum);
    }

    std::string getChecksumWord(const std::vector<std::string> &words)
    {
        std::string trimmed;

        /* Take the first 3 char from each of the 24 words */
        for (const auto &word : words)
        {
            trimmed += word.substr(0, 3);
        }

        /* Hash the data */
        uint64_t hash = crc32::crc32(trimmed);

        /* Modulus the hash by the word length to get the index of the 
           checksum word */
        return words[hash % words.size()];
    }

    std::vector<int> getWordIndexes(const std::vector<std::string> &words)
    {
        std::vector<int> result;

        for (const auto &word : words)
        {
            /* Find the iterator of our word in the wordlist */
            const auto it = std::find(WordList::English.begin(),
                                      WordList::English.end(), word);

            /* Take it away from the beginning of the vector, giving us the
               index of the item in the vector */
            result.push_back(static_cast<int>(std::distance(WordList::English.begin(), it)));
        }

        return result;
    }
}