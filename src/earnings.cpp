//=======================================================================
// Copyright Baptiste Wicht 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "earnings.hpp"
#include "args.hpp"
#include "accounts.hpp"
#include "data.hpp"
#include "guid.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "console.hpp"
#include "budget_exception.hpp"

using namespace budget;

namespace {

static data_handler<earning> earnings;

void show_earnings(boost::gregorian::greg_month month, boost::gregorian::greg_year year){
    std::vector<std::string> columns = {"ID", "Date", "Account", "Name", "Amount"};
    std::vector<std::vector<std::string>> contents;

    money total;
    std::size_t count = 0;

    for(auto& earning : earnings.data){
        if(earning.date.year() == year && earning.date.month() == month){
            contents.push_back({to_string(earning.id), to_string(earning.date), get_account(earning.account).name, earning.name, to_string(earning.amount)});

            total += earning.amount;
            ++count;
        }
    }

    if(count == 0){
        std::cout << "No earnings for " << month << "-" << year << std::endl;
    } else {
        contents.push_back({"", "", "", "Total", to_string(total)});

        display_table(columns, contents);
    }
}

void show_earnings(boost::gregorian::greg_month month){
    auto today = boost::gregorian::day_clock::local_day();

    show_earnings(month, today.year());
}

void show_earnings(){
    auto today = boost::gregorian::day_clock::local_day();

    show_earnings(today.month(), today.year());
}

void show_all_earnings(){
    std::vector<std::string> columns = {"ID", "Date", "Account", "Name", "Amount"};
    std::vector<std::vector<std::string>> contents;

    for(auto& earning : earnings.data){
        contents.push_back({to_string(earning.id), to_string(earning.date), get_account(earning.account).name, earning.name, to_string(earning.amount)});
    }

    display_table(columns, contents);
}

} //end of anonymous namespace

void budget::earnings_module::load(){
    load_earnings();
    load_accounts();
}

void budget::earnings_module::unload(){
    save_earnings();
}

void budget::earnings_module::handle(const std::vector<std::string>& args){
    if(args.size() == 1){
        show_earnings();
    } else {
        auto& subcommand = args[1];

        if(subcommand == "show"){
            if(args.size() == 2){
                show_earnings();
            } else if(args.size() == 3){
                show_earnings(boost::gregorian::greg_month(to_number<unsigned short>(args[2])));
            } else if(args.size() == 4){
                show_earnings(
                    boost::gregorian::greg_month(to_number<unsigned short>(args[2])),
                    boost::gregorian::greg_year(to_number<unsigned short>(args[3])));
            } else {
                throw budget_exception("Too many arguments to earning show");
            }
        } else if(subcommand == "all"){
            show_all_earnings();
        } else if(subcommand == "add"){
            earning earning;
            earning.guid = generate_guid();
            earning.date = boost::gregorian::day_clock::local_day();

            if(args.size() == 2){
                edit_date(earning.date, "Date");

                std::string account_name;
                edit_string(account_name, "Account");
                validate_account(account_name);
                earning.account = get_account(account_name).id;

                edit_string(earning.name, "Name");
                not_empty(earning.name, "The name of the earning cannot be empty");

                edit_money(earning.amount, "Amount");
                not_negative(earning.amount);
            } else {
                enough_args(args, 5);

                auto account_name = args[2];
                validate_account(account_name);
                earning.account = get_account(account_name).id;

                earning.amount = parse_money(args[3]);
                not_negative(earning.amount);

                for(std::size_t i = 4; i < args.size(); ++i){
                    earning.name += args[i] + " ";
                }

                not_empty(earning.name, "The name of the earning cannot be empty");
            }

            add_data(earnings, std::move(earning));
        } else if(subcommand == "addd"){
            enough_args(args, 6);

            earning earning;
            earning.guid = generate_guid();
            earning.date = boost::gregorian::from_string(args[2]);

            auto account_name = args[3];
            validate_account(account_name);
            earning.account = get_account(account_name).id;

            earning.amount = parse_money(args[4]);
            not_negative(earning.amount);

            for(std::size_t i = 5; i < args.size(); ++i){
                earning.name += args[i] + " ";
            }

            not_empty(earning.name, "The name of the earning cannot be empty");

            add_data(earnings, std::move(earning));
        } else if(subcommand == "delete"){
            enough_args(args, 3);

            std::size_t id = to_number<std::size_t>(args[2]);

            if(!exists(earnings, id)){
                throw budget_exception("There are no earning with id ");
            }

            remove(earnings, id);

            std::cout << "earning " << id << " has been deleted" << std::endl;
        } else if(subcommand == "edit"){
            enough_args(args, 3);

            std::size_t id = to_number<std::size_t>(args[2]);

            if(!exists(earnings, id)){
                throw budget_exception("There are no earning with id " + args[2]);
            }

            auto& earning = get(earnings, id);

            edit_date(earning.date, "Date");

            auto account_name = get_account(earning.account).name;
            edit_string(account_name, "Account");
            validate_account(account_name);
            earning.account = get_account(account_name).id;

            edit_string(earning.name, "Name");
            not_empty(earning.name, "The name of the earning cannot be empty");

            edit_money(earning.amount, "Amount");
            not_negative(earning.amount);

            std::cout << "earning " << id << " has been modified" << std::endl;
        } else {
            throw budget_exception("Invalid subcommand \"" + subcommand + "\"");
        }
    }
}

void budget::load_earnings(){
    load_data(earnings, "earnings.data");
}

void budget::save_earnings(){
    save_data(earnings, "earnings.data");
}

std::ostream& budget::operator<<(std::ostream& stream, const earning& earning){
    return stream << earning.id  << ':' << earning.guid << ':' << earning.account << ':' << earning.name << ':' << earning.amount << ':' << to_string(earning.date);
}

void budget::operator>>(const std::vector<std::string>& parts, earning& earning){
    earning.id = to_number<std::size_t>(parts[0]);
    earning.guid = parts[1];
    earning.account = to_number<std::size_t>(parts[2]);
    earning.name = parts[3];
    earning.amount = parse_money(parts[4]);
    earning.date = boost::gregorian::from_string(parts[5]);
}

std::vector<earning>& budget::all_earnings(){
    return earnings.data;
}