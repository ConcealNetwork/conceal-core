/*
Copyright (C) 2018, The Conceal developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <cctype>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <Common/ConsoleTools.h>
#include <Common/StringTools.h>

#include <CryptoNoteCore/TransactionExtra.h>

#include <SimpleWallet/PasswordContainer.h>

void confirmPassword(std::string walletPass);

bool confirm(std::string msg);

std::string formatAmount(uint64_t amount);
std::string formatDollars(uint64_t amount);
std::string formatCents(uint64_t amount);

std::string getPaymentID(std::string extra);

class ColouredMsg
{
    public:
        ColouredMsg(std::string msg, Common::Console::Color colour) 
                  : msg(msg), colour(colour) {}

        ColouredMsg(std::string msg, int padding, 
                    Common::Console::Color colour)
                  : msg(msg), colour(colour), padding(padding), pad(true) {}


        /* Set the text colour, write the message, then reset. We use a class
           as it seems the only way to have a valid << operator. We need this
           so we can nicely do something like:

           std::cout << "Hello " << GreenMsg("user") << std::endl;

           Without having to write:

           std::cout << "Hello ";
           GreenMsg("user");
           std::cout << std::endl; */

        friend std::ostream& operator<<(std::ostream& os, const ColouredMsg &m)
        {
            Common::Console::setTextColor(m.colour);

            if (m.pad)
            {
                os << std::left << std::setw(m.padding) << m.msg;
            }
            else
            {
                os << m.msg;
            }

            Common::Console::setTextColor(Common::Console::Color::Default);
            return os;
        }

    protected:
        std::string msg;
        Common::Console::Color colour;
        int padding = 0;
        bool pad = false;
};

class SuccessMsg : public ColouredMsg
{
    public:
        explicit SuccessMsg(std::string msg) 
               : ColouredMsg(msg, Common::Console::Color::Green) {}

        explicit SuccessMsg(std::string msg, int padding)
               : ColouredMsg(msg, padding, Common::Console::Color::Green) {}
};

class InformationMsg : public ColouredMsg
{
    public:
        explicit InformationMsg(std::string msg) 
               : ColouredMsg(msg, Common::Console::Color::BrightYellow) {}

        explicit InformationMsg(std::string msg, int padding)
               : ColouredMsg(msg, padding, 
                             Common::Console::Color::BrightYellow) {}
};

class SuggestionMsg : public ColouredMsg
{
    public:
        explicit SuggestionMsg(std::string msg) 
               : ColouredMsg(msg, Common::Console::Color::BrightYellow) {}

        explicit SuggestionMsg(std::string msg, int padding)
               : ColouredMsg(msg, padding, 
                             Common::Console::Color::BrightYellow) {}
};

class WarningMsg : public ColouredMsg
{
    public:
        explicit WarningMsg(std::string msg) 
               : ColouredMsg(msg, Common::Console::Color::BrightRed) {}

        explicit WarningMsg(std::string msg, int padding)
               : ColouredMsg(msg, padding, 
                             Common::Console::Color::BrightRed) {}
};

/* This borrows from haskell, and is a nicer boost::optional class. We either
   have Just a value, or Nothing.

   Example usage follows.
   The below code will print:

   ```
   100
   Nothing
   ```

   Maybe<int> parseAmount(std::string input)
   {
        if (input.length() == 0)
        {
            return Nothing<int>();
        }

        try
        {
            return Just<int>(std::stoi(input)
        }
        catch (const std::invalid_argument)
        {
            return Nothing<int>();
        }
   }

   int main()
   {
       auto foo = parseAmount("100");

       if (foo.isJust)
       {
           std::cout << foo.x << std::endl;
       }
       else
       {
           std::cout << "Nothing" << std::endl;
       }

       auto bar = parseAmount("garbage");

       if (bar.isJust)
       {
           std::cout << bar.x << std::endl;
       }
       else
       {
           std::cout << "Nothing" << std::endl;
       }
   }

*/

template <class X> struct Maybe
{
    X x;
    bool isJust;

    Maybe(const X &x) : x (x), isJust(true) {}
    Maybe() : isJust(false) {}
};

template <class X> Maybe<X> Just(const X&x)
{
    return Maybe<X>(x);
}

template <class X> Maybe<X> Nothing()
{
    return Maybe<X>();
}
