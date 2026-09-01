#!/usr/bin/perl -w
##########################################################################################
# vim: fileencoding=utf8:encoding=utf8
##########################################################################################

#### FIXME Only for test FIXME ####
#  export FRITZ_BOX_BUILD_DIR=/home/blabitzke/fbox/GU_RELEASE
#  export KERNEL_LAYOUT=iks
#### FIXME Only for test FIXME ####
use utf8;
use strict;
use diagnostics;    # For verbose warning messages
use Carp qw(cluck); # For debugging purposes
use POSIX qw(:termios_h);
use Term::ANSIColor qw(:constants);
$Term::ANSIColor::AUTORESET = 1;
use sigtrap qw(die normal-signals);
use Tie::File;
if(-e "$ENV{FRITZ_BOX_BUILD_DIR}/Generate/shell/read_config.pm") {
    use lib "$ENV{FRITZ_BOX_BUILD_DIR}/Generate/shell";
    use read_config;
}
#use Data::Dumper;
#use generic_names;

my $KERNEL_CLASS = $ARGV[0];
my $DRIVER_NAME = $ARGV[1];

print "KERNEL_CLASS = '$KERNEL_CLASS'\n"; # FIXME
print "DRIVER_NAME  = '$DRIVER_NAME'\n";  # FIXME

if($KERNEL_CLASS eq "") {
    $KERNEL_CLASS = 26;
}

unless(-e "./Makefile.$KERNEL_CLASS") {
    exit 0; # No architecture specific Makefile available
}
unlink("./Makefile");
system("ln -sf ./Makefile.$KERNEL_CLASS ./Makefile");
system("ln -fs `pwd`/linux_avm_cpmac.h ../../../include/linux/avm_cpmac.h");
system("ln -fs `pwd`/linux_adm_reg.h ../../../include/linux/adm_reg.h");
system("ln -fs `pwd`/linux_ar_reg.h ../../../include/linux/ar_reg.h");


##########################################################################################
#
##########################################################################################
my %product_data = ();
my $max_hwrev_length;
my $max_phys;


##########################################################################################
# Helper function for read_config.pm
##########################################################################################
sub verbose {
#    my ($section, $level, $text) = @_;
#    if($level > 8) {
#        print $text;
#    }
}


##########################################################################################
# Check configuration
##########################################################################################
sub check_configuration {
    my %phy_types = ();
    my %phy_modes = ();

    # Read the header file to obtain the legal configurations
    open(HEADER, "<cpmac_product_conf.h"); 
    while(<HEADER>) {
        # Read the numeral constants
        if(/\/\* init_cpmac defines \*\//) {
            $_ = <HEADER>;
            $_ =~ s/\#define AVM_CPMAC_MAX_HWREV_LENGTH ([0-9]*.*)/$1/;
            $max_hwrev_length = $_;
            $_ = <HEADER>;
            $_ =~ s/\#define AVM_CPMAC_MAX_PHYS ([0-9]*).*/$1/;
            $max_phys = $_;
        }
        # Read the possible PHY types
        if(/\/\* init_cpmac PHY_TYPES \*\//) {
            $_ = <HEADER>; # Skip typedef enum
            while(<HEADER>) {
                if(/^\} cpmac_phy_types;/) {
                    last;
                }
                $_ =~ s/ *CPMAC_PHY_TYPE_([^, ]*)[, ]*/$1/g;
                $_ =~ s/\n//;
                $_ = lc($_);
#                print "'$_'\n";
                $phy_types{$_} = 1;
            }
        }
        # Read the possible PHY modes
        if(/\/\* init_cpmac PHY_MODES \*\//) {
            $_ = <HEADER>; # Skip typedef enum
            while(<HEADER>) {
                if(/^\} cpmac_phy_configs;/) {
                    last;
                }
                $_ =~ s/ *CPMAC_PHY_MODE_([^, ]*)[, ]*/$1/g;
                $_ =~ s/\n//;
                $_ = lc($_);
#                print "'$_'\n";
                $phy_modes{$_} = 1;
            }
        }
    }
    close(HEADER);

    HWREV: foreach my $hwrev (sort { (($a =~ /^\d+$/) && ($b =~ /^\d+$/)) ? $a <=> $b : $a cmp $b } keys %product_data) {
#        print "Checking HWrev $hwrev\n"; # FIXME
        $product_data{$hwrev}{"valid"} = 1;
        PHYS: foreach my $phy (sort keys %{$product_data{$hwrev}{"phys"}}) {
            if(exists $product_data{$hwrev}{"phys"}{$phy}{"type"}) {
#                print "Type: '" . $product_data{$hwrev}{"phys"}{$phy}{"type"} . "'\n"; # FIXME
                if(!exists $phy_types{$product_data{$hwrev}{"phys"}{$phy}{"type"}}) {
                    print "init_cpmac.pl: Unknown PHY type '" . $product_data{$hwrev}{"phys"}{$phy}{"type"} . "' given for " . $product_data{$hwrev}{"name"} . " ($hwrev)!\n";
                    $product_data{$hwrev}{"valid"} = 0;
                    next HWREV;
                }
                if($product_data{$hwrev}{"phys"}{$phy}{"type"} eq "none") {
                    next PHYS;
                }
                unless(exists $product_data{$hwrev}{"phys"}{$phy}{"config"}) {
                    $product_data{$hwrev}{"phys"}{$phy}{"config"} = "default";
                }
                # Do we have a switch? Then we need a mode configuration
                if(   ($product_data{$hwrev}{"phys"}{$phy}{"type"} eq "adm6996l")
                   || ($product_data{$hwrev}{"phys"}{$phy}{"type"} eq "adm6996fc")
                   || ($product_data{$hwrev}{"phys"}{$phy}{"type"} eq "tantos")
                   || ($product_data{$hwrev}{"phys"}{$phy}{"type"} eq "ar8216")
                   || ($product_data{$hwrev}{"phys"}{$phy}{"type"} eq "ar8316")) {
                    unless(   (exists $product_data{$hwrev}{"phys"}{$phy}{"config"})
                           && (exists $phy_modes{$product_data{$hwrev}{"phys"}{$phy}{"config"}})
                           && (exists $product_data{$hwrev}{"phys"}{$phy}{"ports"})
                           && ($product_data{$hwrev}{"phys"}{$phy}{"config"} ne "default")) {
                        $product_data{$hwrev}{"valid"} = 0;
                        print "init_cpmac.pl: Missing or wrong data for switch configuration given for " . $product_data{$hwrev}{"name"} . " ($hwrev)!\n";
                        next HWREV;
                    }
                    unless($product_data{$hwrev}{"phys"}{$phy}{"ports"} =~ /0x[[:xdigit:]]*/) {
                        $product_data{$hwrev}{"valid"} = 0;
                        print "init_cpmac.pl: Wrong port configuration given for " . $product_data{$hwrev}{"name"} . " ($hwrev)!\n";
                        next HWREV;
                    }
                }
                # External switches/PHYs need a reset pin
                if($product_data{$hwrev}{"phys"}{$phy}{"type"} ne "internal") { # "none" was skipped already
                    if(exists $product_data{$hwrev}{"phys"}{$phy}{"rst"}) {
                        unless($product_data{$hwrev}{"phys"}{$phy}{"rst"} =~ /[[:digit:]]*/) {
                            $product_data{$hwrev}{"valid"} = 0;
                            print "init_cpmac.pl: Wrong reset bit configuration given for " . $product_data{$hwrev}{"name"} . " ($hwrev)!\n";
                            next HWREV;
                        }
                    } else {
                        $product_data{$hwrev}{"phys"}{$phy}{"rst"} = 0xffff;
                    }
                }
            }
        }
    }
}


##########################################################################################
# Generate configuration source code
##########################################################################################
sub write_configuration {
    my $phy;
    my $hwrevkomma;
    my $phykomma;

    open(CONF, ">cpmac_product_conf.c"); 
    print CONF '#include "cpphy_types.h"' . "\n\n";
    print CONF "cpmac_product_struct cpmac_products = {\n";
    print CONF "    " . scalar (keys %product_data) . ",\n";
    print CONF "    {\n";
    $hwrevkomma = 0;
    foreach my $hwrev (sort { (($a =~ /^\d+$/) && ($b =~ /^\d+$/)) ? $a <=> $b : $a cmp $b } keys %product_data) {
        next unless($product_data{$hwrev}{"valid"});
        print CONF ",\n" if($hwrevkomma);
        print CONF "        { /* " . $product_data{$hwrev}{"name"} . " */\n";
        print CONF "            \"$hwrev \",\n";
        print CONF "            " . scalar (keys %{$product_data{$hwrev}{"phys"}}) . ",\n";

#        printf "HWRev = % 3s: '%-28s'   ", $hwrev, $product_data{$hwrev}{"name"};
#        foreach $phy (sort keys %{$product_data{$hwrev}{"phys"}}) {
#            printf "PHY %s: %-10s %-7s %-9s %-13s ", 
#                   $phy,
#                   (exists $product_data{$hwrev}{"phys"}{$phy}{"type"}) ? $product_data{$hwrev}{"phys"}{$phy}{"type"} : "",
#                   (exists $product_data{$hwrev}{"phys"}{$phy}{"rst"}) ? "rst " . $product_data{$hwrev}{"phys"}{$phy}{"rst"} : "",
#                   (exists $product_data{$hwrev}{"phys"}{$phy}{"config"}) ? $product_data{$hwrev}{"phys"}{$phy}{"config"} : "",
#                   (exists $product_data{$hwrev}{"phys"}{$phy}{"ports"}) ? "ports " . $product_data{$hwrev}{"phys"}{$phy}{"ports"} : "";
#        }
#        print "\n";

        $phykomma = 0;
        print CONF "            {\n";
        for($phy = 0; $phy < $max_phys; $phy++) {
            print CONF ",\n" if($phykomma);
            if(exists $product_data{$hwrev}{"phys"}{$phy}{"type"}) {
                $product_data{$hwrev}{"phys"}{$phy}{"type"} = "CPMAC_PHY_TYPE_" . uc($product_data{$hwrev}{"phys"}{$phy}{"type"});
            } else {
                $product_data{$hwrev}{"phys"}{$phy}{"type"} = "CPMAC_PHY_TYPE_NONE";
            }
            if(exists $product_data{$hwrev}{"phys"}{$phy}{"config"}) {
                $product_data{$hwrev}{"phys"}{$phy}{"config"} = "CPMAC_PHY_MODE_" . uc($product_data{$hwrev}{"phys"}{$phy}{"config"});
            } else {
                $product_data{$hwrev}{"phys"}{$phy}{"config"} = "CPMAC_PHY_MODE_DEFAULT";
            }
            $product_data{$hwrev}{"phys"}{$phy}{"ports"} = "0x1" unless(exists $product_data{$hwrev}{"phys"}{$phy}{"ports"});
            $product_data{$hwrev}{"phys"}{$phy}{"rst"} = "0xffff" unless(exists $product_data{$hwrev}{"phys"}{$phy}{"rst"});
            $product_data{$hwrev}{"phys"}{$phy}{"mii"} = "0x0" unless(exists $product_data{$hwrev}{"phys"}{$phy}{"mii"});
            print CONF "                {\n";
            print CONF "                    " . $product_data{$hwrev}{"phys"}{$phy}{"type"} . ",\n";
            print CONF "                    " . $product_data{$hwrev}{"phys"}{$phy}{"config"} . ",\n";
            print CONF "                    " . $product_data{$hwrev}{"phys"}{$phy}{"ports"} . ",\n";
            print CONF "                    " . $product_data{$hwrev}{"phys"}{$phy}{"rst"} . ",\n";
            print CONF "                    " . $product_data{$hwrev}{"phys"}{$phy}{"mii"} . "\n";
            print CONF "                }";
            $phykomma = 1;
        }
        print CONF "\n            }\n";
        print CONF "        }";
        $hwrevkomma = 1;
    }
    print CONF "\n    }\n";
    print CONF "};\n";
    close(CONF);
}


##########################################################################################
# Main init routine
##########################################################################################
sub init {
    my %config_select = ();
    my %config_data = ();
    my $hwrev;

    Generate::shell::read_config::init_configs(\%config_select, \%config_data);
    Generate::shell::read_config::read_configs("$ENV{FRITZ_BOX_BUILD_DIR}/arch");
#    $Data::Dumper::Indent = 3;         # pretty print with array indices
#    print Dumper(%config_data);
    foreach my $box (sort keys %{$config_data{produkte}}) {
        next if($box eq "name");
        next if($box eq "produkt_list");

        my $error = 0;
        my $phys = 0;

        unless(exists $config_data{produkte}{$box}{params}{"CPMAC_INIT"}) {
            write_configuration();
            return; # No CPMAC_INIT yet, therefor the old configuration is needed
        }

        $hwrev = $config_data{produkte}{$box}{params}{"HWREV"};
        if(exists $product_data{$hwrev}) {
            print "\n*******************************************************************************\n";
            print "***** ATTENTION! The HWRev $hwrev is not unique! Ignoring the later entries! *****\n";
            print "*******************************************************************************\n\n";
            next;
        }

        $product_data{$hwrev}{"name"} = $box;
        $product_data{$hwrev}{"init"} = $config_data{produkte}{$box}{params}{"CPMAC_INIT"};
        $product_data{$hwrev}{"valid"} = 0;
        foreach my $entry (split " ", $config_data{produkte}{$box}{params}{"CPMAC_INIT"}) {
            if($entry =~ /ignore_device/) {
                # This entry is intentionally ignored
                delete $product_data{$hwrev};
                next;
            }
            if($entry =~ /xxxcpmacxxx/) {
#                print "init_cpmac.pl: No configuration for $box ($hwrev).\n";
                delete $product_data{$hwrev};
                next;
            }
            if($entry =~ /phy([0-9]*)_(.*)\((.*)\)/) {
                $product_data{$hwrev}{"phys"}{lc($1)}{lc($2)} = lc($3);
#                print "Found $1 $2 $3\n";
#            } else {
#                print "***** Attention! No ethernet configuration for '$box' (HWRev $hwrev) *****\n";
            }
        }

#        print "'$box' (HWRev = " . $config_data{produkte}{$box}{params}{"HWREV"} 
#                  . ") CPMAC_INIT = " . $config_data{produkte}{$box}{params}{"CPMAC_INIT"} . "\n";
    }
    check_configuration();
    write_configuration();
}


#/*------------------------------------------------------------------------------------------*\
#\*------------------------------------------------------------------------------------------*/
if(-e "$ENV{FRITZ_BOX_BUILD_DIR}/Generate/shell/read_config.pm") {
    init();
    print "\n\n";
} else {
    print "Error! Missing $ENV{FRITZ_BOX_BUILD_DIR}/Generate/shell/read_config.pm\n\n";
    exit 10;
}

