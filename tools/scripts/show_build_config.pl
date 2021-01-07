#!/usr/bin/env perl

use strict;
use FileHandle;
use File::Basename;

sub file_header {
  my $filename = basename(shift);
  '=' x 16 . " $filename " . '=' x 16;
}

sub show_file {
  my $filename = shift;
  my $callback = shift;
  return unless -r $filename;
  print file_header($filename), "\n";
  my $f = new FileHandle($filename);
  while (<$f>) {
    print;
    &$callback($_) if $callback;
  }
  close $f;
}

sub show_config {
  my $setenv = shift;
  my $dds_root = dirname($setenv);
  my $ace_root;
  show_file($setenv, sub {$ace_root = $1 if $_[0] =~ /ACE_ROOT=(.*)/} );
  die "ACE_ROOT not found in setenv" unless $ace_root;

  my $user_macros = ($dds_root eq '.' ? '' : "$dds_root/") . 'user_macros.GNU';
  show_file($user_macros);

  my $opendds_version = ($dds_root eq '.' ? '' : "$dds_root/") . 'VERSION.txt';
  show_file($opendds_version);

  show_file("$ace_root/VERSION.txt");

  show_file("$ace_root/bin/MakeProjectCreator/config/default.features");
  show_file("$ace_root/include/makeinclude/platform_macros.GNU");
  show_file("$ace_root/ace/config.h");
}

if (-r 'setenv.sh') {
  show_config('setenv.sh');
}
elsif (-r 'setenv.cmd') {
  show_config('setenv.cmd');
}
elsif (-r 'build/host/setenv.sh') {
  print "Host Build:\n";
  show_config('build/host/setenv.sh');
  print "Target Build:\n";
  show_config('build/target/setenv.sh');
}
else {
  die "Can't find setenv script that was generated by configure.\n" .
    "Make sure this is being run in root of OpenDDS.\n";
}
