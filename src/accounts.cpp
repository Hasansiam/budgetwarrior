//=======================================================================
// Copyright (c) 2013-2014 Baptiste Wicht.
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "accounts.hpp"
#include "budget_exception.hpp"
#include "args.hpp"
#include "data.hpp"
#include "guid.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "console.hpp"
#include "earnings.hpp"
#include "expenses.hpp"

using namespace budget;

namespace {

static data_handler<account> accounts;

void show_accounts(){
    std::vector<std::string> columns = {"ID", "Name", "Amount"};
    std::vector<std::vector<std::string>> contents;

    money total;

    for(auto& account : accounts.data){
        if(account.until == budget::date(2099,12,31)){
            contents.push_back({to_string(account.id), account.name, to_string(account.amount)});

            total += account.amount;
        }
    }

    contents.push_back({"", "Total", to_string(total)});

    display_table(columns, contents);
}

void show_all_accounts(){
    std::vector<std::string> columns = {"ID", "Name", "Amount", "Since", "Until"};
    std::vector<std::vector<std::string>> contents;

    for(auto& account : accounts.data){
        contents.push_back({to_string(account.id), account.name, to_string(account.amount), to_string(account.since), to_string(account.until)});
    }

    display_table(columns, contents);
}

budget::date find_new_since(){
    budget::date date(1400,1,1);

    for(auto& account : all_accounts()){
        if(account.until != budget::date(2099,12,31)){
            if(account.until - days(1) > date){
                date = account.until - days(1);
            }
        }
    }

    return date;
}

} //end of anonymous namespace

void budget::accounts_module::load(){
    load_accounts();
    load_expenses();
    load_earnings();
}

void budget::accounts_module::unload(){
    save_accounts();
    save_expenses();
    save_earnings();
}

void budget::accounts_module::handle(const std::vector<std::string>& args){
    if(args.size() == 1){
        show_accounts();
    } else {
        auto& subcommand = args[1];

        if(subcommand == "show"){
            show_accounts();
        } else if(subcommand == "all"){
            show_all_accounts();
        } else if(subcommand == "add"){
            account account;
            account.guid = generate_guid();
            account.since = find_new_since();
            account.until = budget::date(2099,12,31);

            edit_string(account.name, "Name", not_empty_checker());
            edit_money(account.amount, "Amount", not_negative_checker());

            if(account_exists(account.name)){
                throw budget_exception("An account with this name already exists");
            }

            auto id = add_data(accounts, std::move(account));
            std::cout << "Account " << id << " has been created" << std::endl;
        } else if(subcommand == "delete"){
            enough_args(args, 3);

            std::size_t id = to_number<std::size_t>(args[2]);

            if(!exists(accounts, id)){
                throw budget_exception("There are no account with id " + args[2]);
            }

            for(auto& expense : all_expenses()){
                if(expense.account == id){
                    throw budget_exception("There are still some expenses linked to this account, cannot delete it");
                }
            }

            for(auto& earning : all_earnings()){
                if(earning.account == id){
                    throw budget_exception("There are still some earnings linked to this account, cannot delete it");
                }
            }

            remove(accounts, id);

            std::cout << "Account " << id << " has been deleted" << std::endl;
        } else if(subcommand == "edit"){
            enough_args(args, 3);

            std::size_t id = to_number<std::size_t>(args[2]);

            if(!exists(accounts, id)){
                throw budget_exception("There are no account with id " + args[2]);
            }

            auto& account = get(accounts, id);

            edit_string(account.name, "Name", not_empty_checker());
            edit_money(account.amount, "Amount", not_negative_checker());

            //TODO Verify that there are no OTHER account with this name
            //in the current set of accounts (taking archiving into account)

            std::cout << "Account " << id << " has been modified" << std::endl;

            accounts.changed = true;
        } else if(subcommand == "archive"){
            std::cout << "This command will create new accounts that will be used starting from the beginning of the current month. Are you sure you want to proceed ? [yes/no] ? ";

            std::string answer;
            std::cin >> answer;

            if(answer == "yes" || answer == "y"){
                std::vector<std::size_t> sources;
                std::vector<budget::account> copies;

                auto today = budget::local_day();

                auto tmp = budget::local_day() - months(1);
                budget::date until_date(tmp.year(), tmp.month(), tmp.end_of_month().day());

                for(auto& account : all_accounts()){
                    if(account.until == budget::date(2099,12,31)){
                        budget::account copy;
                        copy.guid = generate_guid();
                        copy.name = account.name;
                        copy.amount = account.amount;
                        copy.until = budget::date(2099,12,31);
                        copy.since = until_date + days(1);

                        account.until = until_date;

                        copies.push_back(std::move(copy));

                        sources.push_back(account.id);
                    }
                }

                std::unordered_map<std::size_t, std::size_t> mapping;

                for(std::size_t i = 0; i < copies.size(); ++i){
                    auto& copy = copies[i];

                    auto id = add_data(accounts, std::move(copy));

                    mapping[sources[i]] = id;
                }

                for(auto& expense : all_expenses()){
                    if(expense.date.month() == today.month() && expense.date.year() == today.year()){
                        if(mapping.find(expense.account) != mapping.end()){
                            expense.account = mapping[expense.account];
                            set_expenses_changed();
                        }
                    }
                }

                for(auto& earning : all_earnings()){
                    if(earning.date.month() == today.month() && earning.date.year() == today.year()){
                        if(mapping.find(earning.account) != mapping.end()){
                            earning.account = mapping[earning.account];
                            set_earnings_changed();
                        }
                    }
                }

                accounts.changed = true;
            }
        } else {
            throw budget_exception("Invalid subcommand \"" + subcommand + "\"");
        }
    }
}

void budget::load_accounts(){
    load_data(accounts, "accounts.data");
}

void budget::save_accounts(){
    save_data(accounts, "accounts.data");
}

budget::account& budget::get_account(std::size_t id){
    return get(accounts, id);
}

budget::account& budget::get_account(std::string name, budget::year year, budget::month month){
    budget::date date(year, month, 5);

    for(auto& account : accounts.data){
        if(account.since < date && account.until > date && account.name == name){
            return account;
        }
    }

    budget_unreachable("The account does not exist");
}

std::ostream& budget::operator<<(std::ostream& stream, const account& account){
    return stream << account.id  << ':' << account.guid << ':' << account.name << ':' << account.amount << ':' << to_string(account.since) << ':' << to_string(account.until);
}

void budget::operator>>(const std::vector<std::string>& parts, account& account){
    account.id = to_number<std::size_t>(parts[0]);
    account.guid = parts[1];
    account.name = parts[2];
    account.amount = parse_money(parts[3]);
    account.since = from_string(parts[4]);
    account.until = from_string(parts[5]);
}

bool budget::account_exists(const std::string& name){
    for(auto& account : accounts.data){
        if(account.name == name){
            return true;
        }
    }

    return false;
}

std::vector<account>& budget::all_accounts(){
    return accounts.data;
}

std::vector<account> budget::all_accounts(budget::year year, budget::month month){
    std::vector<account> accounts;

    budget::date date(year, month, 5);

    for(auto& account : all_accounts()){
        if(account.since < date && account.until > date){
            accounts.push_back(account);
        }
    }

    return std::move(accounts);
}
