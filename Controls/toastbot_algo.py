"""
An adaptive toaster control system that adjusts heating power in real time
based on color sensor feedback. This script receives RGB values from an Arduino
via serial communication, converts them to expected R values using mathematical
models, and adjusts power to ensure the toast reaches the desired browning level.

Features:
- Adaptive power control using 100%, 80%, and 60% levels
- Goldening and browning phase modeling
- Serial logging and real-time power adjustment
- Predictive and reactive toast doneness detection
"""

import serial
import time
import csv
import re
import math

# Constants
PORT = "COM5"
BAUD_RATE = 9600
TIMEOUT = 1

# Equation models for different power levels
GOLDENING_100_EQUATION = lambda t: 43.29 * (1 - math.exp(-0.03184 * t)) + 244.79
BROWNING_100_EQUATION = lambda t: -0.07551 * t + 311.22

GOLDENING_80_EQUATION = lambda t: 62.32 * (1 - math.exp(-0.02597 * t)) + 139.48
BROWNING_80_EQUATION = lambda t: -0.01715 * t + 195.10

LINEAR_60_EQUATION = lambda t: -0.01086 * t + 136.70

RATE_DIFF_100 = 1.10401
RATE_DIFF_80 = 0.55556

DEFAULT_POWER = 80
TARGET_R_VALUE = None

GOLDENING_THRESHOLD = 285.0
ADJUSTMENT_CHECK_INTERVAL = 1.0
MAX_ADJUSTMENT_TIME = 10.0


class ToasterController:
    def init(self, port, baud_rate, timeout):
        self.ser = serial.Serial(port, baud_rate, timeout=timeout)
        time.sleep(2)  # Wait for serial connection to establish
        self.start_time = None
        self.current_power = DEFAULT_POWER
        self.phase = "goldening"  # Start in goldening phase
        self.adjustment_time_left = 0
        self.output_file = "toaster_control_log.csv"

        # For R value counting
        self.r_values_buffer = []
        self.target_r_count = 0  # Counter for readings at/below target
        self.readings_needed = (
            5  # Number of readings needed at/below target (not necessarily consecutive)
        )
        self.phase_transition_time = None
        self.counting_active = (
            False  # Flag to indicate if we've started counting target readings
        )

        # For minimum duration check
        self.initial_estimated_finish_time = None
        self.minimum_ending_time_buffer = (
            60  # Seconds before estimated time when ending is allowed
        )

        # For tracking effective time at 80% power
        self.effective_time_80_equiv = 0
        self.last_time_check = None
        self.power_effectiveness = {
            100: 1.1,  # 100% power is 1.1 times more effective than 80%
            80: 1.0,  # Baseline
            60: 0.55,  # 60% power is 0.55 times as effective as 80%
        }

        # For stabilization after power changes
        self.stabilization_needed = False
        self.stabilization_time_left = 0
        self.stabilization_period = 3.0  # Seconds to wait after returning to 80% power

        # Initialize CSV log
        with open(self.output_file, "w", newline="") as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow(
                [
                    "TimeElapsed",
                    "PowerLevel",
                    "Phase",
                    "R",
                    "G",
                    "B",
                    "ExpectedR",
                    "Difference",
                    "Action",
                    "EffectiveTime",
                ]
            )

        print("Toaster Control System Initialized")


def set_power(self, power_level):
    """Send power level command to Arduino"""
    self.ser.write(f"{power_level}\n".encode())
    self.current_power = power_level
    print(f"Power level set to {power_level}%")


def calculate_expected_r(self, elapsed_time):
    """Calculate expected R value based on current phase and time"""
    try:
        if self.phase == "goldening":
            if self.current_power == 100:
                if (
                    elapsed_time <= 258.15
                ):  # Use the correct time threshold for 100% power
                    return GOLDENING_100_EQUATION(elapsed_time)
                else:
                    return BROWNING_100_EQUATION(elapsed_time)
            else:  # 80% power
                if elapsed_time <= GOLDENING_THRESHOLD:
                    return GOLDENING_80_EQUATION(elapsed_time)
                else:
                    return BROWNING_80_EQUATION(elapsed_time)
        else:  # browning phase
            if self.current_power == 100:
                return BROWNING_100_EQUATION(
                    elapsed_time - self.phase_transition_time + 258.15
                )
            elif self.current_power == 80:
                # In browning phase, time is relative to the start of browning
                time_in_browning = elapsed_time - self.phase_transition_time
                return BROWNING_80_EQUATION(GOLDENING_THRESHOLD + time_in_browning)
            else:  # 60% power
                return LINEAR_60_EQUATION(elapsed_time)
    except Exception as e:
        print(f"Error calculating expected R: {e}")
        # Return a reasonable default value
        return 150  # Middle of typical R range


def r_to_time_remaining(self, r_value):
    """Convert R value to estimated browning time remaining using 80% power equation"""
    # Using the browning equation: R(t) = -0.01715*t + 195.10
    # Solve for t: t = (195.10 - R) / 0.01715
    time_remaining = (195.10 - r_value) / 0.01715
    # Ensure we don't return negative time values
    return max(60, time_remaining)  # At least 60 seconds as a safety


def calculate_adjustment_time(self, difference, to_power):
    """Calculate time needed for power adjustment based on rate differences"""
    if to_power == 100:
        # Using rate difference for 100% power
        calculated_time = abs(difference) / RATE_DIFF_100
    else:  # to_power == 60
        # Using rate difference for 60% power
        calculated_time = abs(difference) / RATE_DIFF_80

    # Limit adjustment time to maximum allowed
    return min(calculated_time, MAX_ADJUSTMENT_TIME)


def adjust_power(self, current_r, expected_r):
    """Determine if power adjustment is needed and for how long"""
    difference = current_r - expected_r

    # If we're in a stabilization period after returning to 80% power
    if self.stabilization_needed and self.current_power == 80:
        self.stabilization_time_left -= ADJUSTMENT_CHECK_INTERVAL
        if self.stabilization_time_left <= 0:
            print("\n===== STABILIZATION COMPLETE =====")
            print(f"Sensor readings have stabilized, resuming normal control")
            self.stabilization_needed = False
        else:
            print(
                f"Stabilization period: {self.stabilization_time_left:.1f}s remaining"
            )
            print(f"(R values not used for control decisions during stabilization)")
        return "Waiting for sensor readings to stabilize"

    # If we're already in an adjustment period, continue with it without checking R values
    if self.adjustment_time_left > 0:
        self.adjustment_time_left -= ADJUSTMENT_CHECK_INTERVAL
        if self.adjustment_time_left <= 0:
            print("\n===== ADJUSTMENT COMPLETE =====")
            print(f"Returning to 80% power baseline")
            self.set_power(80)
            # Start stabilization period
            self.stabilization_needed = True
            self.stabilization_time_left = self.stabilization_period
            print(f"Starting {self.stabilization_period}s stabilization period")
        else:
            print(
                f"Remaining time at {self.current_power}% power: {self.adjustment_time_left:.1f}s"
            )
            print(
                f"(Note: R values during power adjustment are not used for control decisions)"
            )
        return "Completing power adjustment period"

    # Only check if adjustment is needed when at baseline power (80%) and not in stabilization
    if self.current_power == 80 and not self.stabilization_needed:
        # Check if R value is behind or ahead of prediction
        if difference < -5:  # R value is behind (lower than expected)
            # Increase power to catch up
            self.set_power(100)
            self.adjustment_time_left = self.calculate_adjustment_time(difference, 100)
            print(f"\n===== POWER ADJUSTMENT =====")
            print(
                f"R value behind by {abs(difference):.2f}, increasing power to 100% for {self.adjustment_time_left:.2f}s"
            )
            print(f"(Maximum adjustment time limited to {MAX_ADJUSTMENT_TIME}s)")
            print(f"Will return to 80% baseline after adjustment period")
            return f"Increasing power to 100% for {self.adjustment_time_left:.2f}s"
        elif difference > 5:  # R value is ahead (higher than expected)
            # Decrease power to slow down
            self.set_power(60)
            self.adjustment_time_left = self.calculate_adjustment_time(difference, 60)
            print(f"\n===== POWER ADJUSTMENT =====")
            print(
                f"R value ahead by {difference:.2f}, decreasing power to 60% for {self.adjustment_time_left:.2f}s"
            )
            print(f"(Maximum adjustment time limited to {MAX_ADJUSTMENT_TIME}s)")
            print(f"Will return to 80% baseline after adjustment period")
            return f"Decreasing power to 60% for {self.adjustment_time_left:.2f}s"
        else:
            return "On track, maintaining 80% power"
    else:
        return f"Maintaining current power at {self.current_power}%"


def update_effective_time(self, current_time):
    """Update the effective time at 80% power equivalent based on current power level"""
    if self.last_time_check is None:
        self.last_time_check = current_time
        return self.effective_time_80_equiv

    # Calculate elapsed time since last check
    elapsed = current_time - self.last_time_check

    # Convert to effective time based on current power level
    effectiveness = self.power_effectiveness.get(self.current_power, 1.0)
    effective_elapsed = elapsed * effectiveness

    # Add to total effective time
    self.effective_time_80_equiv += effective_elapsed

    # Update last check time
    self.last_time_check = current_time

    return self.effective_time_80_equiv


def check_phase_transition(self, elapsed_time, current_time):
    """Check if we should transition from goldening to browning phase based on effective time"""
    # Update effective time at 80% power
    effective_time = self.update_effective_time(current_time)

    if self.phase == "goldening" and effective_time > GOLDENING_THRESHOLD:
        self.phase = "browning"
        self.phase_transition_time = elapsed_time
        print(f"\n===== TRANSITION: Goldening phase completed =====")
        print(f"Real time elapsed: {elapsed_time:.2f}s")
        print(f"Effective time at 80% power: {effective_time:.2f}s")
        print(f"Entering browning phase\n")
        return True
    return False


def estimate_phase_times(self, current_r):
    """Estimate remaining time in current phase and total time"""
    if self.phase == "goldening":
        # Estimate remaining time in goldening phase
        # Using the 80% power equation, elapsed time at transition will be GOLDENING_THRESHOLD
        effective_time = self.effective_time_80_equiv
        remaining_goldening_time = max(0, GOLDENING_THRESHOLD - effective_time)

        # Estimate total browning time based on current R value
        # At the transition point, the R value should be around the goldening end value
        # We can use the browning equation to estimate from there
        transition_r = GOLDENING_80_EQUATION(GOLDENING_THRESHOLD)

        # Make sure the transition R value is reasonable
        # If R is above the browning equation's y-intercept, the equation will give negative time
        if transition_r > 195.10:  # This is the y-intercept from the browning equation
            # Use a default estimate based on typical browning time
            browning_time = 300  # Default to 5 minutes
        else:
            browning_time = self.r_to_time_remaining(transition_r)
            browning_time = max(60, browning_time)  # Ensure at least 60 seconds

        return remaining_goldening_time, browning_time
    else:
        # Already in browning phase
        remaining_time = self.r_to_time_remaining(current_r)
        # Ensure positive time estimate
        return 0, max(60, remaining_time)  # At least 60 seconds remaining


def control_loop(self):
    """Main control loop for the toaster"""
    print("Starting control loop with baseline power at 80%...")
    self.start_time = time.time()
    self.last_time_check = self.start_time  # Initialize time tracking
    self.set_power(DEFAULT_POWER)

    # Calculate initial estimate for total process time
    initial_goldening_time = GOLDENING_THRESHOLD
    # Estimate browning time using the goldening end value
    transition_r = GOLDENING_80_EQUATION(GOLDENING_THRESHOLD)
    initial_browning_time = self.r_to_time_remaining(transition_r)

    # Make sure estimates are reasonable
    if initial_browning_time < 0:
        print("Warning: Calculated negative browning time. Using default value.")
        initial_browning_time = 300  # Default to 5 minutes

    # Set the estimated finish time
    total_estimated_time = initial_goldening_time + initial_browning_time
    self.initial_estimated_finish_time = self.start_time + total_estimated_time

    print(f"\n===== PROCESS TIMELINE =====")
    print(
        f"Estimated goldening time: {initial_goldening_time:.1f}s at 80% power equivalent"
    )
    print(f"Estimated browning time: {initial_browning_time:.1f}s")
    print(f"Total estimated time: {total_estimated_time:.1f}s")
    print(
        f"Process will not end until at least {total_estimated_time - self.minimum_ending_time_buffer:.1f}s have elapsed"
    )
    print(f"Maximum power adjustment time: {MAX_ADJUSTMENT_TIME}s")
    print(f"Stabilization period after power changes: {self.stabilization_period}s")
    print(f"Power effectiveness ratios:")
    print(f"  100% power = {self.power_effectiveness[100]:.2f}x effective as 80% power")
    print(f"  80% power = {self.power_effectiveness[80]:.2f}x (baseline)")
    print(f"  60% power = {self.power_effectiveness[60]:.2f}x effective as 80% power")
    print("===============================\n")

    try:
        while True:
            # Read data from sensor
            line = self.ser.readline().decode("utf-8").strip()

            if line.startswith("Ambient:"):
                match_rgb = re.search(
                    r"Ambient: (\d+) Red: (\d+) Green: (\d+) Blue: (\d+)", line
                )
                if match_rgb:
                    ambient, r_str, g, b = match_rgb.groups()
                    r = int(r_str)

                    # Read the LAB color values (but we mainly use RGB)
                    lab_line = self.ser.readline().decode("utf-8").strip()
                    match_lab = re.search(
                        r"L: ([\d\.]+) a: ([\d\.]+) b: ([\d\.]+)", lab_line
                    )

                    if match_lab:
                        L, a, b_lab = match_lab.groups()

                        current_time = time.time()
                        elapsed_time = current_time - self.start_time
                        elapsed_time = round(elapsed_time, 2)

                        # Update effective time at 80% power equivalent
                        effective_time = self.update_effective_time(current_time)

                        # Check if we need to transition from goldening to browning
                        self.check_phase_transition(elapsed_time, current_time)

                        # Calculate expected R value based on equations
                        expected_r = self.calculate_expected_r(elapsed_time)
                        difference = r - expected_r

                        # Determine if power adjustment is needed (only when at 80% power)
                        action = self.adjust_power(r, expected_r)

                        # Estimate phase times
                        (
                            remaining_goldening,
                            remaining_browning,
                        ) = self.estimate_phase_times(r)

                        # Don't use R values for control decisions when not at 80% power
                        # or during stabilization period after power changes
                        valid_r_for_control = (
                            self.current_power == 80 and not self.stabilization_needed
                        )

                        # Store R value in buffer (for backward compatibility/debugging)
                        if valid_r_for_control:
                            self.r_values_buffer.append(r)
                            if len(self.r_values_buffer) > self.readings_needed:
                                self.r_values_buffer.pop(0)  # Keep buffer at fixed size

                        # Display status with time remaining until allowed to end
                        current_time = time.time()
                        if self.initial_estimated_finish_time is not None:
                            time_until_estimated_finish = max(
                                0, self.initial_estimated_finish_time - current_time
                            )
                            if (
                                time_until_estimated_finish
                                > self.minimum_ending_time_buffer
                            ):
                                print(
                                    f"Time until target counting begins: {time_until_estimated_finish - self.minimum_ending_time_buffer:.1f}s"
                                )
                            elif self.counting_active:
                                print(
                                    f"Target counting active: {self.target_r_count}/{self.readings_needed} readings at/below target"
                                )
                            else:
                                print(
                                    f"Ready to begin target counting (minimum time requirement met)"
                                )

                        # Display status
                        print(
                            f"\nt={elapsed_time:.1f}s | Power={self.current_power}% | Phase={self.phase} | R={r}"
                        )
                        print(f"Effective time at 80% power: {effective_time:.1f}s")

                        if valid_r_for_control:
                            print(
                                f"Expected R={expected_r:.1f} | Diff={difference:.1f} | R data valid for control"
                            )
                        elif self.stabilization_needed:
                            print(
                                f"R data not used for control - in {self.stabilization_time_left:.1f}s stabilization period"
                            )
                        else:
                            print(
                                f"R data not used for control during power adjustment"
                            )

                        # Display phase-specific information
                        if self.phase == "goldening":
                            print(
                                f"Goldening phase: {elapsed_time:.1f}s elapsed, estimated {remaining_goldening:.1f}s remaining"
                            )
                            print(
                                f"Expected browning time after goldening: {remaining_browning:.1f}s"
                            )
                            print(
                                f"Total estimated time remaining: {remaining_goldening + remaining_browning:.1f}s"
                            )
                        else:  # browning phase
                            time_in_browning = elapsed_time - self.phase_transition_time
                            print(
                                f"Browning phase: {time_in_browning:.1f}s elapsed, estimated {remaining_browning:.1f}s remaining"
                            )
                            print(
                                f"Total time so far: {elapsed_time:.1f}s (Goldening: {self.phase_transition_time:.1f}s, Browning: {time_in_browning:.1f}s)"
                            )

                        # If in power adjustment, show remaining adjustment time
                        if self.adjustment_time_left > 0:
                            print(
                                f"Power adjustment: {self.current_power}% for {self.adjustment_time_left:.1f}s more"
                            )

                        # Log data
                        with open(self.output_file, "a", newline="") as csvfile:
                            writer = csv.writer(csvfile)
                            writer.writerow(
                                [
                                    elapsed_time,
                                    self.current_power,
                                    self.phase,
                                    r,
                                    g,
                                    b,
                                    round(expected_r, 2),
                                    round(difference, 2),
                                    action,
                                    round(effective_time, 2),
                                ]
                            )

                        # Check if we've reached the target R value (non-consecutive counting)
                        if (
                            TARGET_R_VALUE is not None
                            and self.phase == "browning"
                            and valid_r_for_control
                        ):
                            # Check if we're close enough to the estimated finish time to start counting
                            current_time = time.time()
                            time_until_estimated_finish = max(
                                0, self.initial_estimated_finish_time - current_time
                            )
                            count_allowed = (
                                time_until_estimated_finish
                                <= self.minimum_ending_time_buffer
                            )

                            # If we're within the target time window, start counting
                            if count_allowed and not self.counting_active:
                                self.counting_active = True
                                self.target_r_count = 0
                                print(f"\n===== ENTERING TARGET COUNTING PHASE =====")
                                print(
                                    f"Now within {self.minimum_ending_time_buffer} seconds of estimated finish time"
                                )
                                print(
                                    f"Starting to count readings at/below target R={TARGET_R_VALUE}"
                                )

                            # If current reading is at or below target and we're counting, increment counter
                            if self.counting_active and r <= TARGET_R_VALUE:
                                self.target_r_count += 1
                                print(
                                    f"Target R count: {self.target_r_count}/{self.readings_needed} readings at/below target"
                                )

                            # Debug output
                            if self.counting_active:
                                print(
                                    f"Current R={r}, Target R={TARGET_R_VALUE}, Count={self.target_r_count}/{self.readings_needed}"
                                )

                            # Check if we've reached the required number of readings
                            if (
                                self.counting_active
                                and self.target_r_count >= self.readings_needed
                            ):
                                print(f"\n===== TARGET REACHED =====")
                                print(
                                    f"Detected {self.target_r_count} readings at/below target R={TARGET_R_VALUE}"
                                )
                                print(
                                    f"Total time: {elapsed_time:.1f}s (Goldening: {self.phase_transition_time:.1f}s, Browning: {elapsed_time - self.phase_transition_time:.1f}s)"
                                )
                                print("Stopping heating elements.")
                                self.set_power(0)  # Turn off heating
                                break

            # Brief pause to prevent excessive CPU usage
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("\nToaster control stopped by user.")
        self.set_power(0)  # Turn off heating when stopping

    finally:
        self.ser.close()
        print("Serial connection closed.")


def main():
    global TARGET_R_VALUE


print("===== Toaster Control System =====")
print("This system controls toasting based on color sensor readings.")

# Get target R value from user
try:
    TARGET_R_VALUE = int(
        input("Enter target R value (lower means darker toast, typically 120-160): ")
    )
    print(f"Target R value set to {TARGET_R_VALUE}")
    print(
        f"Note: The toaster will stop after detecting {5} readings at/below this target"
    )
    print(
        "      (not necessarily consecutive), and only after entering the browning phase"
    )
    print("      and being within 60 seconds of the estimated finish time.")
except ValueError:
    print("Invalid input. Using default tracking without target.")
    TARGET_R_VALUE = None

# Initialize and start controller
controller = ToasterController(PORT, BAUD_RATE, TIMEOUT)
controller.control_loop()

if __name__ == "__main__":
    main()
