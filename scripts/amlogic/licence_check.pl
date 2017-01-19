#!/usr/bin/perl -W
#
# licence_check.pl V0.10
#
# jianxin.pan@2016-11-25


use File::Basename;
use File::Find;

$sc_dir = File::Spec->rel2abs(dirname( "$0") ) ;
$sc_dir =~ s/\/scripts\/amlogic//;
my $top = "$sc_dir";

my $nofix = 0;
my $failno = 0;
my $shname = $0;
#@ARGV=("../../include/linux/amlogic","../../drivers/amlogic" ) ;
my @path;
for(@ARGV)
{
	my $dir	=$_;
	if(/^\//)
	{
	}
	elsif(/--nofix/)
	{
		$nofix = 1;
		next;
	}
	else
	{
		$dir = File::Spec->rel2abs($dir);
		#print "\n Real2abs Dir: --$dir-- \n";
	}
	push(@path,$dir);
}


my $licence=
"/*
 * File_name_here
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */\n\n";


#print $licence;

sub licence_process
{
	my ($file_name) = @_;
	my $d = dirname($file_name);
	my $f = basename($file_name);
	#print "\n Abs <$d>,  f_ $f";
	#print "\n Top: <$top>  ";
	my $log_name = File::Spec->abs2rel($d, $top);
	$log_name =$log_name."\/";
	$log_name .=basename($file_name);
	my $len = length($log_name);
	if($len > 76)
	{
		$log_name =basename($file_name);
		$len = length($log_name);
		if($len > 76)
		{
			$log_name ="";
		}
	}
	#print "\n Process: ",$log_name;
	$licence_0=$licence;
	$licence_0=~s/File_name_here/$log_name/;
	my $count = 0;
	my $text_0="";
	my $text_all=$licence_0;
	open(my $f_in, '<', $file_name) or die "Can't Open $file_name: For Read \n";
	my ($left,$right, $lineno,$space) = (0, 0, 0,0);
	while ($line = <$f_in>)
	{
		$text_0 .= $line;
		#Empty Line or Line marked by //
		if(($space==0) &&(($line =~/^\s*$/)||
		(($line =~/^\s*\/\//)&&($line !~ /\*\//))))
		{
			#print "\n Line $lineno is empty.";
		}
		elsif(($space==0) &&($line =~ /^\s*\/\*/))											#Match /*
		{
			$left ++;
			#print "\n L Matched: $lineno  $line, $left  ";								#Match that /* and */ in the same line
			if($line =~ /\*\//)
			{
				$right ++;
				#print "\n L Matched: $lineno  $line, $left  ";
			}
		}
		elsif(($space==0) &&($line =~ /\*\//)&& ($line !~ /\/\*/) )													#Match */
		{
			$right ++;
			#print "\n R Matched: $lineno  $line, $right  ";
			if($left == $right)
			{
				$space = 1;
			}
		}
		elsif($left==$right)	#Content Lines
		{
			if(($line =~/^\s*$/)&& ($count==0))
			{

			}
			else
			{
				#print $line;
				$space = 1;
				$count +=1;
				$text_all .=$line;
			}
		}
		$lineno++;
	}
	close($f_in);

	if($text_0 ne $text_all)
	{
		$failno ++;
		if($nofix)
		{
			print "\n  Licence_WARN: <";
			print File::Spec->abs2rel($file_name, $top).">\n";;
		}
		else
		{
			print "\n  Licence_FIXED: <";
			print File::Spec->abs2rel($file_name, $top).">\n";;
			open(my $f_out, '>', $file_name)
			or die "Can't Open $file_name\n";
			print $f_out $text_all;
			close $f_out;
		}
	}
	$text_all='';
}


my ($c_cnt, $h_cnt) = (0, 0);
sub process
{
    my $file = $File::Find::name;
    if (-f $file)
     {
		if(($file =~ /.*\.[CchH]$/i))
				{
					$c_cnt++;
				licence_process($file);
			}
		if(($file =~ /.*\.dts$/i))
				{
					$c_cnt++;
				licence_process($file);
			}
		if(($file =~ /.*\.dtsi$/i))
				{
					$c_cnt++;
				licence_process($file);
			}
	}
}

for(@path)
{
	#print "\n Fine $_ \n";
	find(\&process, $_);
}
