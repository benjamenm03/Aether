// Copyright 2020, the Aether Development Team (see doc/dev_team.md for members)
// Full license can be found in License.md

// Versions and authors:
//
// initial version - A. Ridley - Sometime in 2020
// Temperature dependent reactions - M. Rinaldi - 2022
// Headers and Perturbations - Y Jiang / A. Ridley - June 2023
// Error Checking - S. Loucks - August 2023
//

#include <string>
#include <fstream>
#include <vector>
#include <iostream>

#include "aether.h"

// -----------------------------------------------------------------------------
// Initialize chemistry class
// -----------------------------------------------------------------------------

Chemistry::Chemistry(Neutrals neutrals,
                     Ions ions) {

  std::string function = "Chemistry::Chemistry";
  static int iFunction = -1;
  report.enter(function, iFunction);

  if (read_chemistry_file(neutrals, ions) > 0) {
    report.print(0, "Could not read chemistry file!");
    throw std::invalid_argument( "Invalid chemistry file" );
  }

  report.exit(function);
  return;
}

// -----------------------------------------------------------------------------
// Read chemistry CSV file
// -----------------------------------------------------------------------------
bool Chemistry::search(std::string name,
                       json &headers,
                       std::vector<std::string> &error) {
  if (!headers.contains(name)) {
    error.push_back(name);
    return false;
  }

  return true;
}

bool Chemistry::check_chemistry_file(json &headers,
                                     std::vector<std::vector<std::string>> csv,
                                     Report &report) {
  std::string function = "Chemistry::check_chemistry_file";
  static int iFunction = -1;
  report.enter(function, iFunction);

  std::vector<std::string> error;
  bool IsOk = true;

  //check non-essential headers so that we don't call a non-existant
  //column in checking values
  std::vector<std::string> non_essentials = {
    "uncertainty", "Piecewise", "tempdependent", "FormulaType",
    "Denominator", "Numerator", "Exponent", "Temprange", "Min", "Max"
  };
  //create list of existing non-essential headers
  std::vector<std::string> exists;

  for (std::string head : non_essentials)
    exists.push_back(search(head, headers, error) ? head : "");

  error.clear();

  // Check for columns that are essential, have "loss_something",
  // "source_something", "rate", "branching", "heat"
  for (int i = 1; i < 4; ++i) {
    //check loss
    std::string title = "loss" + std::to_string(i);

    if (!search(title, headers, error))
      IsOk = false;

    //check source
    std::string title2 = "source" + std::to_string(i);

    if (!search(title2, headers, error))
      IsOk = false;
  }

  //check other essential headers
  if (!search("rate", headers, error))
    IsOk = false;

  if (!search("branching", headers, error))
    IsOk = false;

  if (!search("heat", headers, error))
    IsOk = false;

  //output missing headers & clear cache of errors
  if (!IsOk) {
    std::string error_message = "Errors in chemistry header, missing: \n";

    for (std::string err : error) {
      report.print(0, err);
      error_message += err + " ";
    }

    report.print(0, "are missing from the Chemistry file header.");
    report.error(error_message);
    report.exit(function);
    return false;
  }

  error.clear();

  //check individual values in the csv
  for (int iLine = 2; iLine < csv.size(); iLine++) {
    bool temp_ok = true;

    if (csv[0].size() != csv[iLine].size() && iProc == 0) {
      std::cout << "There are " << headers.size()
                << " headers but " << csv[iLine].size()
                << " columns in line " << iLine << ".\n";
      report.exit(function);
      return false;
    }

    if (csv[iLine][headers["rate"]].length() > 0) {
      std::string col;

      //check loss & source columns are elements or electrons
      //(check if the first character is a letter)
      bool loss1 = csv[iLine][headers["loss1"]] == "";
      bool source1 = csv[iLine][headers["source1"]] == "";

      for (int num = 1; num < 4; num++) {
        col = "loss" + std::to_string(num);

        if (csv[iLine][headers[col]] != "") {
          if (!std::isalpha(csv[iLine][headers[col]][0]) || (loss1 && num > 1)) {
            temp_ok = false;
            error.push_back(col);
          }
        }

        col = "source" + std::to_string(num);

        if (csv[iLine][headers[col]] != "") {
          if (!std::isalpha(csv[iLine][headers[col]][0]) ||
              (source1 && num > 1)) {
            temp_ok = false;
            error.push_back(col);
          }
        }
      }

      //check rate column is a double
      col = "rate";

      try {
        stod(csv[iLine][headers[col]]);
      } catch (std::invalid_argument & e) {
        temp_ok = false;
        error.push_back(col);
      }

      //check piecewise values are either Ti or Tn
      std::string piece = "";
      col = "Piecewise";

      if (find(exists.begin(), exists.end(), col) != exists.end()) {
        piece = csv[iLine][headers[col]];

        if (piece != "")
          if (!(piece == "Ti" || piece == "Tn")) {
            temp_ok = false;
            error.push_back(col);
          }
      }

      //check branching column is a double between 0 & 1
      col = "branching";

      if (csv[iLine][headers[col]] != "") {
        int ratio;

        try {
          ratio = stod(csv[iLine][headers[col]]);

          if (ratio > 1 || ratio < 0) {
            temp_ok = false;
            error.push_back(col);
          }
        } catch (std::invalid_argument & e) {
          temp_ok = false;
          error.push_back(col);
        }

      }

      //check heat column is a double
      col = "heat";

      if (csv[iLine][headers[col]] != "") {
        try {
          stod(csv[iLine][headers[col]]);
        } catch (std::invalid_argument & e) {
          temp_ok = false;
          error.push_back(col);
        }
      }

      //check uncertainty column is a double
      col = "uncertainty";

      if (find(exists.begin(), exists.end(), col) != exists.end()) {
        if (csv[iLine][headers[col]] != "") {
          try {
            stod(csv[iLine][headers[col]]);
          } catch (std::invalid_argument & e) {
            temp_ok = false;
            error.push_back(col);
          }
        }
      }

      //check the Temp Dependent column fits the formulas provided
      col = "tempdependent";

      if ( find(exists.begin(), exists.end(), col) != exists.end()) {
        std::string value = csv[iLine][headers[col]];

        if (value != "") {
          std::string min = csv[iLine][headers["Temprange"]];

          if (csv[iLine][headers["FormulaType"]] == "") {
            //check that there is a formula type provided
            temp_ok = false;
            error.push_back("FormulaType");

          } else if (csv[iLine][headers["FormulaType"]] == "1") {
            // there are two formulas for type 1 -- one is if there is a
            // minimum in the temp range, the other if not either
            // formula is allowed here
            std::string formula1 =
              "(" + csv[iLine][headers["Numerator"]] +
              "/" + csv[iLine][headers["Denominator"]] + ")";
            std::string formula2 =
              "(" + csv[iLine][headers["Denominator"]] + "/" +
              csv[iLine][headers["Numerator"]] + ")";

            //check exponent & exponent of formula is a double
            bool exp_match = false;

            try {
              exp_match =
                std::abs(stod(csv[iLine][headers["Exponent"]])) ==
                std::abs(stod(value.substr(value.find("^") + 1)));
            } catch (std::invalid_argument & e) {
              temp_ok = false;
              error.push_back("Exponent");
            }

            //check if the absolute value of the exponents match & the
            //rest of the formula matches too
            if (!(value.substr(0, value.find("^")) == formula1 ||
                  value.substr(0, value.find("^")) == formula2) ||
                !exp_match) {
              temp_ok = false;
              error.push_back(col);
            }

          } else if (csv[iLine][headers["FormulaType"]] == "2") {
            // this is for formula type 2, there weren't many examples,
            // so it might be a restrictive formula
            std::string formula =
              csv[iLine][headers["Denominator"]] +
              "*exp(" + csv[iLine][headers["Numerator"]] + "/";
            formula += csv[iLine][headers["Denominator"]] + ")";

            if (csv[iLine][headers[col]] != formula) {
              temp_ok = false;
              error.push_back(col);
            }
          }
        }
      }

      // check formula type is an integer and equals either 1 or 2
      col = "FormulaType";

      if (find(exists.begin(), exists.end(), col) != exists.end()) {
        if (csv[iLine][headers[col]] != "") {
          try {
            if (!(stoi(csv[iLine][headers[col]]) == 1 ||
                  stoi(csv[iLine][headers[col]]) == 2)) {
              temp_ok = false;

              if (find(error.begin(), error.end(), col) == error.end())
                error.push_back(col);
            }
          } catch (std::invalid_argument & e) {
            temp_ok = false;

            if (find(error.begin(), error.end(), col) == error.end())
              error.push_back(col);
          }

          if (csv[iLine][headers["tempdependent"]] == "") {
            temp_ok = false;

            if (find(error.begin(), error.end(), col) == error.end())
              error.push_back(col);
          }
        }
      }

      // check if the temp function fits the format (ex: Ti<20)
      col = "Temprange";

      if (find(exists.begin(), exists.end(), col) != exists.end()) {
        std::string function = csv[iLine][headers[col]];

        if (function != "") {
          bool inequality =
            function.find(">") != std::string::npos ||
            function.find("<") != std::string::npos;
          bool has_number =
            function.find(csv[iLine][headers["Min"]]) != std::string::npos ||
            function.find(csv[iLine][headers["Max"]]) != std::string::npos;
          bool has_denom =
            csv[iLine][headers["Piecewise"]] == "" ||
            csv[iLine][headers["Piecewise"]] == function.substr(0, 2);

          if (!has_denom || !inequality || !has_number) {
            temp_ok = false;
            error.push_back(col);
          }
        }
      }
    }

    //report errors when they are encountered, also update the
    //function variable IsOk
    if (!temp_ok) {
      std::string error_message =
        "There is an issue with the Chemistry csv file, on line "
        + std::to_string(iLine + 1) + ", with columns:\n ";

      for (std::string err : error) {
        error_message =
          error_message + err + ": " + csv[iLine][headers[err]] + "\n";
      }

      std::cout << "check chem file error : " << error_message;
      report.error(error_message);
      report.print(0, error_message);
      IsOk = false;
      error.clear();
    }
  }

  report.exit(function);
  return IsOk;
}

int Chemistry::read_chemistry_file(Neutrals neutrals,
                                   Ions ions) {

  std::string function = "Chemistry::read_chemistry_file";
  static int iFunction = -1;
  report.enter(function, iFunction);

  std::ifstream infile_ptr;
  int iErr = 0;
  reaction_type reaction;

  std::vector<std::string> errors;

  report.print(1, "Reading Chemistry File : " + input.get_chemistry_file());
  infile_ptr.open(input.get_chemistry_file());

  if (!infile_ptr.is_open()) {
    report.print(0, "Could not open chemistry file!");
    iErr = 1;
    throw std::invalid_argument( "Invalid chemistry file" );
  } else {

    if (infile_ptr.good()) {

      std::vector<std::vector<std::string>> csv = read_csv(infile_ptr);

      int nLines = csv.size();

      if (nLines <= 2) {
        iErr = 1;
        report.print(0, "Chemistry file doesn't contain data!");
        throw std::invalid_argument( "Invalid chemistry file" );
      } else {

        json headers;

        for (int x = 0; x < csv[0].size(); ++x)
          headers[csv[0][x]] = x;

        // Check before here, then set rate & loss1
        if (!search("rate", headers, errors) || !search("loss1", headers, errors)) {
          iErr = 1;
          report.print(0, "Missing rate or loss columns for Chemistry file!");
          throw std::invalid_argument( "Invalid chemistry file" );
        }

        int iRate_ = headers["rate"];
        int iLoss1_ = headers["loss1"];

        if (!check_chemistry_file(headers, csv, report)) {
          iErr = 1;
          throw std::invalid_argument( "Invalid chemistry file" );
        }

        nReactions = 0;

        // Skip 2 lines of headers!
        for (int iLine = 2; iLine < nLines; iLine++) {
          // Some final rows can have comments in them, so we want to
          // skip anything where the length of the string in column 2
          // is == 0:
          if (csv[iLine][iRate_].length() > 0) {
            report.print(3, "interpreting chemistry line : " +
                         csv[iLine][headers["name"]]);

            reaction = interpret_reaction_line(neutrals, ions,
                                               csv[iLine], headers);

            // This section perturbs the reaction rates
            // (1) if the user specifies a value in the "uncertainty" column of the
            //     chemistry.csv file;
            // (2) if the user asks for it in the ["Perturb"]["Chemistry"] part
            //     of the aether.json file
            if (headers.contains("uncertainty")) {
              if (csv[iLine][headers["uncertainty"]].length() > 0) {
                // uncertainty column exists!
                json values = input.get_perturb_values();

                if (values.contains("Chemistry")) {
                  json chemistryList = values["Chemistry"];

                  if (chemistryList.size() > 0) {

                    // loop through requested pertubations:
                    for (auto& react : chemistryList) {
                      if (react == "all" || react == reaction.name) {
                        precision_t perturb_rate =
                          str_to_num(csv[iLine][headers["uncertainty"]]);

                        int seed = input.get_updated_seed();
                        std::vector<double> perturbation;
                        precision_t mean = 1.0;
                        precision_t std = perturb_rate;
                        int nV = 1;

                        perturbation = get_normal_random_vect(mean,
                                                              std,
                                                              nV,
                                                              seed);

                        if (report.test_verbose(2))
                          std::cout << "Perturbing reaction "
                                    << reaction.name << " by multiplier : "
                                    << perturbation[0] << "\n";

                        reaction.rate *= perturbation[0];
                        break;
                      } // check for react
                    } // chemistry list
                  } // if there were any reactions listed
                } // if user requested perturbs of chemistry
              } // if there was a value in the uncertainty column for the reaction
            } // if there is an uncertainty column in the chemisty csv file
          } // if there is actually a reaction rate

          // check if it is part of a piecewise function,
          //   if so use sources/losses for last reaction
          if (reaction.nLosses == 0 && reaction.nSources == 0) {
            reaction.sources_names = reactions.back().sources_names;
            reaction.losses_names = reactions.back().losses_names;

            reaction.sources_ids = reactions.back().sources_ids;
            reaction.losses_ids = reactions.back().losses_ids;

            reaction.sources_IsNeutral = reactions.back().sources_IsNeutral;
            reaction.losses_IsNeutral = reactions.back().losses_IsNeutral;

            reaction.nLosses = reactions.back().nLosses;
            reaction.nSources = reactions.back().nSources;

            reaction.branching_ratio = reactions.back().branching_ratio;

            reaction.energy = reactions.back().energy;

            reaction.piecewiseVar = reactions.back().piecewiseVar;
          }

          if (reaction.nLosses > 0 && reaction.nSources > 0) {
            if (report.test_verbose(3))
              display_reaction(reaction);

            reactions.push_back(reaction);
            nReactions++;
          }
        }
      }
    } else {
      report.print(0, "Could not open good chemistry file!");
      iErr = 1;
      throw std::invalid_argument( "Invalid chemistry file" );
    }
  }

  report.exit(function);
  return iErr;
}

// -----------------------------------------------------------------------------
// Interpret a comma separated line of the chemical reaction file
// -----------------------------------------------------------------------------

Chemistry::reaction_type Chemistry::interpret_reaction_line(Neutrals neutrals,
                                                            Ions ions,
                                                            std::vector<std::string> line,
                                                            json headers) {

  std::string function = "Chemistry::interpret_reaction_line";
  static int iFunction = -1;
  report.enter(function, iFunction);

  reaction_type reaction;

  int i;
  int id_;
  bool IsNeutral;

  // Losses (left side) first:
  reaction.nLosses = 0;

  for (i = headers["loss1"]; i < headers["loss3"]; i++) {
    find_species_id(line[i], neutrals, ions, id_, IsNeutral);

    if (id_ >= 0) {
      reaction.losses_names.push_back(line[i]);
      reaction.losses_ids.push_back(id_);
      reaction.losses_IsNeutral.push_back(IsNeutral);
      reaction.nLosses++;
    }
  }

  // Sources (right side) second:
  reaction.nSources = 0;

  for (i = headers["source1"]; i < headers["source3"]; i++) {
    find_species_id(line[i], neutrals, ions, id_, IsNeutral);

    if (id_ >= 0) {
      reaction.sources_names.push_back(line[i]);
      reaction.sources_ids.push_back(id_);
      reaction.sources_IsNeutral.push_back(IsNeutral);
      reaction.nSources++;
    }
  }

  // Reaction Rate:
  reaction.rate = str_to_num(line[headers["rate"]]);

  // Reaction Name:
  reaction.name = line[headers["name"]];

  int iBranch_ = headers["branching"];

  // Branching Ratio:

  if (line[iBranch_].length() > 0)
    reaction.branching_ratio = str_to_num(line[iBranch_]);

  else
    reaction.branching_ratio = 1;

  // energy released as exo-thermic reaction:
  if (line[headers["heat"]].length() > 0)
    reaction.energy = str_to_num(line[headers["heat"]]);
  else
    reaction.energy = 0;

  // default to zero (no piecewise, no exponent)
  reaction.min = 0;
  reaction.max = 0;
  reaction.type = 0;

  // if richards, check for temperature dependence
  if (headers.contains("Numerator")) {
    if (line[headers["Numerator"]].length() > 0) {
      reaction.numerator =  str_to_num(line[headers["Numerator"]]);
      reaction.denominator = line[headers["Denominator"]];

      if (line[headers["Exponent"]].length() > 0)
        reaction.exponent = str_to_num(line[headers["Exponent"]]);

    } else {
      // default to 0 (calc_chemical_sources will use constant rate)
      reaction.type = 0;
    }

    reaction.piecewiseVar = line[headers["Piecewise"]];

    if (line[headers["Min"]].length() > 0)
      reaction.min = str_to_num(line[headers["Min"]]);

    if (line[headers["Max"]].length() > 0)
      reaction.max = str_to_num(line[headers["Max"]]);

    if (line[headers["FormulaType"]].length() > 0)
      reaction.type = int(str_to_num(line[headers["FormulaType"]]));

  }

  report.exit(function);
  return reaction;
}

// -----------------------------------------------------------------------------
// Match a string to the neutral or ion species
// -----------------------------------------------------------------------------

void Chemistry::find_species_id(std::string name,
                                Neutrals neutrals,
                                Ions ions,
                                int &id_,
                                bool &IsNeutral) {

  std::string function = "Chemistry::find_species_id";
  static int iFunction = -1;
  report.enter(function, iFunction);

  int iSpecies;
  IsNeutral = false;

  id_ = neutrals.get_species_id(name);

  if (id_ > -1)
    IsNeutral = true;

  else
    id_ = ions.get_species_id(name);

  report.exit(function);
  return;
}

// -----------------------------------------------------------------------------
// Display a reaction:
// -----------------------------------------------------------------------------

void Chemistry::display_reaction(Chemistry::reaction_type reaction) {

  int i;

  std::cout << "Number of Losses : " << reaction.nLosses << "\n";
  std::cout << "Number of Sources : " << reaction.nSources << "\n";

  for (i = 0; i < reaction.nLosses; i++)
    std::cout << reaction.losses_names[i] << " + ";

  std::cout << " -> ";

  for (i = 0; i < reaction.nSources; i++)
    std::cout << reaction.sources_names[i] << " + ";

  std::cout << " ( RR : " << reaction.rate << ")\n";

  for (i = 0; i < reaction.nLosses; i++)
    std::cout << reaction.losses_ids[i]
              << "(" << reaction.losses_IsNeutral[i] << ")" << " + ";

  std::cout << " -> ";

  for (i = 0; i < reaction.nSources; i++)
    std::cout << reaction.sources_ids[i]
              << "(" << reaction.sources_IsNeutral[i]
              << ")" << " + ";

  std::cout << " ( RR : " << reaction.rate << ")\n";

  if (reaction.type > 0) {
    std::cout << "Temperature Dependence: ("
              << reaction.numerator
              << "/"
              << reaction.denominator
              << ")^"
              << reaction.exponent << "\n";

  }

  if (reaction.min < reaction.max) {
    std::cout << "Range: "
              << reaction.min
              << " < "
              << reaction.piecewiseVar;

    if (reaction.max)
      std::cout << " < " << reaction.max;

    std::cout << "\n";
  }
}
