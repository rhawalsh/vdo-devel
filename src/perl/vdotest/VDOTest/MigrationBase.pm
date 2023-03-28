##
# Provide a basic framework for migrating from one machine to another,
# in order to test changes to hardware, platform, or VDO version.
#
# $Id$
##
package VDOTest::MigrationBase;

use strict;
use warnings FATAL => qw(all);
use English qw(-no_match_vars);
use Log::Log4perl;

use Permabit::Assertions qw(
  assertDefined
  assertMinMaxArgs
  assertNumArgs
);
use Permabit::SupportedVersions qw($SUPPORTED_SCENARIOS $SUPPORTED_VERSIONS);

use base qw(VDOTest);

my $log = Log::Log4perl->get_logger(__PACKAGE__);

#############################################################################
# @paramList{getProperties}
our %PROPERTIES =
  (
   # @ple Audit VDO metadata
   auditVDO   => 1,
   # @ple Number of blocks to write
   blockCount => 33000,
   # @ple Use an upgrade device backed by iscsi, and don't use stripfua
   deviceType => "upgrade-iscsi-linear",
   # @ple Hashref mapping scenario keys to the scenario hash to be used
   scenarios  => undef,
   # @ple Data to share between test methods
   _testData  => undef,
  );
##

#############################################################################
# @inherit
##
sub set_up {
  my ($self) = assertNumArgs(1, @_);
  my @scenarioNames = ( $self->{initialScenario} );

  push(@{$self->{intermediateScenarios}}, 'X86_RHEL9_head');
  push(@scenarioNames, @{$self->{intermediateScenarios}});

  # Verify the test scenarios and versions are valid.
  foreach my $name (@scenarioNames) {
    assertDefined($SUPPORTED_SCENARIOS->{$name}, "Scenario '$name' must be"
                . " defined in SUPPORTED_SCENARIOS to be used.");

    my $versionName = $SUPPORTED_SCENARIOS->{$name}{moduleVersion};
    assertDefined($SUPPORTED_VERSIONS->{$versionName}, "Version '$versionName'"
                . " must be defined in SUPPORTED_VERSIONS to be used.");
  }

  # Generate the scenario hashes and reserve the needed hosts.
  foreach my $name (@scenarioNames) {
    $self->{scenarios}{$name} = $self->generateScenarioHash($name);
  }

  $self->SUPER::set_up();

  # Add the userMachines to the scenario hashes.
  foreach my $name (@scenarioNames) {
    my $scenario = $self->{scenarios}->{$name};
    $scenario->{machine} = $self->getUserMachine($scenario->{hostname});
  }

  my $device = $self->getDevice();
  my $scenario = $self->{scenarios}{$self->{initialScenario}};

  $log->info("Setting up VDO $scenario->{version} on "
             . $scenario->{machine}->getName());
  $device->switchToScenario($scenario);
  $device->setup();
  $device->verifyModuleVersion();
}

#############################################################################
# @inherit
##
sub reserveHosts {
  my ($self) = assertNumArgs(1, @_);
  my @scenarioNames = ($self->{initialScenario},
                       @{$self->{intermediateScenarios}});

  my $classes = {};
  foreach my $name (@scenarioNames) {
    my $scenarioOSClass = $SUPPORTED_SCENARIOS->{$name}{rsvpOSClass};
    my $host = undef;
    my $classString = join(",", $scenarioOSClass, $self->{clientClass});
    ($host) = $self->reserveNumHosts(1, $classString, $self->{clientLabel});
    $self->{scenarios}->{$name}{hostname} = $host;

    my $lcOSClass = lc($scenarioOSClass);
    push(@{$self->{$lcOSClass . "Names"}}, $host);

    if (!grep(/^$lcOSClass$/, @{$self->{typeNames}})) {
      push(@{$self->{typeNames}}, $lcOSClass);
    }
  }

  $self->{defaultHostType} =
    lc($SUPPORTED_SCENARIOS->{$self->{initialScenario}}{rsvpOSClass});
}

#############################################################################
# Test basic read/write and dedupe capability before a migration.
##
sub setupDevice {
  # XXX Placeholder for future change
  return 1;
}

#############################################################################
# Test basic read/write and dedupe capability after a migration.
##
sub verifyDevice {
  # XXX Placeholder for future change
  return 1;
}

#############################################################################
# Move the VDO device to a new scenario.
##
sub switchToIntermediateScenario {
  my ($self, $scenarioName) = assertNumArgs(2, @_);
  my $scenario = $self->{scenarios}{$scenarioName};
  my $device = $self->getDevice();

  $device->stop();
  $device->switchToScenario($scenario);
  $device->start();
}

#############################################################################
# Generate a scenario hash from a scenario specification.
#
# @param scenarioName  The name of the scenario to generate a hash for
#
# @return The scenario hash
##
sub generateScenarioHash {
  my ($self, $scenarioName) = assertNumArgs(2, @_);
  my $version = $SUPPORTED_SCENARIOS->{$scenarioName}{moduleVersion};

  return { name => $scenarioName, version => $version };
}

#############################################################################
# Test basic read/write and dedupe capability after a series of upgrades.
#
# @oparam  dontVerify  don't run the final verification step
##
sub _runTest {
  my ($self, $dontVerify) = assertMinMaxArgs([0], 1, 2, @_);
  my @scenarioList = @{$self->{intermediateScenarios}};
  my $scenarios = join(" to ", @scenarioList);
  $log->info("Test upgrading $self->{initialScenario} to $scenarios");

  $self->setupDevice();
  my $device = $self->getDevice();
  foreach my $intermediateScenario (@scenarioList) {
    if ($device->needsExplicitUpgrade($intermediateScenario)) {
      $log->info("Upgrading to $intermediateScenario explicitly");
      $self->doUpgrade($intermediateScenario);
    } else {
      $log->info("Upgrading to $intermediateScenario implicitly");
      $self->switchToIntermediateScenario($intermediateScenario);
    }
  }
  if (!$dontVerify) {
    $self->verifyDevice();
  }

  $log->info("Returning to $self->{initialScenario}");
  $self->switchToIntermediateScenario($self->{initialScenario});
  # XXX  We should check that the old version still works.
}

1;
