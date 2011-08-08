/***************************************************************************
 *   Copyright (C) 2009-2011 by the resistivity.net development team       *
 *   Carsten Rücker carsten@resistivity.net                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "optionmap.h"
#include <cstdio>

namespace GIMLI{

OptionMap::OptionMap( const std::string & description )
    : descriptionString_( description ) {
    opts_           = NULL;
    lastArgString_  = "";
    lastOption_     = NULL;
    bool help;
    showVersion_ = false;
    
    add( help,    "h" , "help", "This help" );
    add( showVersion_,      "" , "version"             , "Show library version and exit programm." );
}

OptionMap::~OptionMap(){
    for ( std::list < OptionBase * >::iterator it = options_.begin(), itmax = options_.end(); it != itmax; it++ ) delete *it;
    delete lastOption_;
    if ( opts_ ) delete[ ] opts_;
}

void OptionMap::parse( int argc, char * argv[] ){
    int option_char = 0, option_index = 0;

    buildLongOptions();
    //std::cout << allKeys_ << std::endl;
    while ( ( option_char = getopt_long( argc, argv, allKeys_.c_str()
              , opts_, & option_index ) ) != EOF ){

        if ( option_char == 'h' ) {
            printHelp( argv[ 0 ] );
            exit( EXIT_SUCCESS );
        }

        if ( option_char == 0 ){
            LongOptionMap::iterator it = lMap_.find( opts_[ option_index ].name );
            if ( it != lMap_.end() ){
                if ( it->second->name_ == "help" ) {
                    printHelp( argv[ 0 ] );
                    exit( EXIT_SUCCESS );
                }
                it->second->assign( optarg );
            } else {
                //std::cout << "\t not in longmap" << std::endl;
                printHelp( argv[ 0 ] );
            }
        } else {
            CharOptionMap::iterator it = cMap_.find( (char)option_char );
            if ( it != cMap_.end() ){
                it->second->assign( optarg );
            } else {
                //std::cout << "\t not in charmap" << std::endl;
                printHelp( argv[ 0 ] );
            }
        }
    }

    if ( showVersion_ ){
        std::cout << versionStr() << std::endl;
        exit( EXIT_SUCCESS );
    }
    
    std::string lastString;
    for ( int index = optind; index < argc; index++ ){
        lastString += std::string( argv[ index ] );
        if ( index < argc -1 ) lastString += " " ;
    }
    if ( lastOption_ ){
        if ( lastString.empty() ){
            printHelp( argv[ 0 ] );
            exit( EXIT_SUCCESS );
        } else {
            lastOption_->assign( &lastString[ 0 ] );
        }
    } else {
        std::cout << "Non-option argument: " << lastString << std::endl;
    }
}

void OptionMap::buildLongOptions(){
    if ( opts_ ) delete[ ] opts_;

    opts_ = new option[ lMap_.size() + 1 ];
    int count = 0;

    for ( LongOptionMap::iterator it = lMap_.begin(); it != lMap_.end(); it++){
        opts_[ count ].name     = it->first.c_str();
        opts_[ count ].has_arg  = it->second->hasArg_;
        opts_[ count ].val      = 0; //(*it).key();
        opts_[ count ].flag     = 0;
        count ++;
    }

    opts_[ count ].name     = 0;
    opts_[ count ].has_arg  = 0;
    opts_[ count ].val      = 0;
    opts_[ count ].flag     = 0;
}

void OptionMap::printHelp( const std::string & main ){
    std::cout << "Usage: " << main << " [options] " << lastArgString_ << std::endl;
    std::cout << "Description: " << descriptionString_ << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -shortcut [--longname] type (defaultvalue) " << "\t\t\t: General option description" << std::endl<< std::endl;

    for ( std::list < OptionBase * >::iterator it = options_.begin(), itmax = options_.end(); it != itmax; it++ ) {
        std::cout << "  -" << (char)(*it)->key_;
        if ( (*it)->name_.size() > 0 ){
            std::cout << " [ --" << (*it)->name_  << " ]";
        }
        if ( (*it)->hasArg_ == required_argument  ) {
            std::cout << " " << (*it)->typname() << " (" << (*it)->defautToString() << ")";
        } else if ( (*it)->typname() == "int" ){
            std::cout << " incremental (" << (*it)->defautToString() << ")";
        }
        std::cout << "\t\t\t: " << (*it)->help_ << std::endl;
    }
    std::cout << std::endl;
}

} // namespace GIMLI