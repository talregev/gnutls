eval '(exit $?0)' && eval 'exec perl -wS "$0" ${1+"$@"}'
  & eval 'exec perl -wS "$0" $argv:q'
    if 0;

# Copyright (C) 2011-2012 Free Software Foundation, Inc.
#
# This file is part of GnuTLS.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this file; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

$dir = shift;
$param2 = shift;

if ($param2 ne '') {
  $enum = 1;
} else {
  $enum = 0;
}

sub key_of_record {
  local($record) = @_;

  # Split record into lines:
  my @lines = split /\n/, $record;

  my ($i) = 1;
  my ($key) = $lines[$i]; 

  if ($enum == 1) {
    while( !($key =~ m/^\\enumTitle\{(.*)\}/) && ($i < 5)) { $i=$i+1; $key = $lines[$i]; }
  } else {
    while( !($key =~ m/^\\functionTitle\{(.*)\}/) && ($i < 5)) { $i=$i+1; $key = $lines[$i]; }
  }

  return $key;
}

if ($enum == 1) {
  $/="\n\\end{enum}";          # Records are separated by blank lines.
} else {
  $/="\n\\end{function}";          # Records are separated by blank lines.
}
@records = <>;  # Read in whole file, one record per array element.

mkdir $dir;

@records = sort { key_of_record($a) cmp key_of_record($b) } @records;
foreach (@records) {
  $key = $_;
  if ($enum == 1) {
    $key =~ m/\\enumTitle\{(.*)\}/;
    $key = $1;
  } else {
    $key =~ m/\\functionTitle\{(.*)\}/;
    $key = $1;
  }

  $key =~ s/\\_/_/g;
  if (defined $key && $key ne "") {
    open FILE, "> $dir/$key\n" or die $!;
    print FILE $_ . "\n";
    close FILE;
  }
} 

#print @records;
