#!/bin/bash

# Display a message asking if further action is needed
echo "Thank you for downloading this repository!"
echo "Do you need help setting up? (yes/no)"

# Read user input
read input

# Check user input and take appropriate action
if [[ "$input" == "yes" ]]; then
    echo "Please refer to the README file for instructions on how to set up."
elif [[ "$input" == "no" ]]; then
    echo "Great! If you have any questions, feel free to ask."
else
    echo "Invalid input. Please enter 'yes' or 'no'."
fi
