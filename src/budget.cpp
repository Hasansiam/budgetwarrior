//=======================================================================
// Copyright Baptiste Wicht 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#include <string>
#include <iostream>

#include "config.hpp"
#include "args.hpp"
#include "budget_exception.hpp"

#include "debts.hpp"
#include "accounts.hpp"
#include "expenses.hpp"
#include "overview.hpp"

using namespace budget;

int main(int argc, const char* argv[]) {
    std::locale global_locale("");
    std::locale::global(global_locale);
    std::wcout.imbue(global_locale);

    if(!load_config()){
        return 0;
    }

    if(argc == 1){
        month_overview();
    } else {
        auto args = parse_args(argc, argv);

        auto& command = args[0];

        try {
            if(command == "help"){
                std::cout << "Usage: budget command [options]" << std::endl;

                //TODO Display complete help
            } else if(command == "debt"){
                handle_debts(args);
            } else if(command == "account"){
                handle_accounts(args);
            } else if(command == "expense"){
                handle_expenses(args);
            } else if(command == "overview"){
                handle_overview(args);
            } else {
                std::cout << "Unhandled command \"" << command << "\"" << std::endl;

                return 1;
            }
        } catch (const budget_exception& exception){
            std::cout << exception.message() << std::endl;

            return 1;
        }
    }

    return 0;
}
