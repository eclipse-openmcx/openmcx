################################################################################
# Copyright (c) 2021 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

import argparse
import os
import shutil
import stat
import subprocess
import sys
import time
import numpy as np


def _parse_args():
    parser = argparse.ArgumentParser(
        description="Runs examples and compares results with the reference files"
    )

    parser.add_argument("executable", help="path to the OpenMCx executable")
    parser.add_argument("examples", help="path to the folder with the test examples")

    args = parser.parse_args()
    return args


def _clear_readonly_bit(func, path, exc_info):
    os.chmod(path, stat.S_IWRITE)
    func(path)


def _run_tests(exe, test_dir):
    exe_abs_path = os.path.abspath(exe)
    test_dir_abs_path = os.path.abspath(test_dir)

    test_examples = [example for example in os.listdir(test_dir_abs_path) if os.path.isfile(os.path.join(test_dir_abs_path, example, "model.ssd"))]

    num_tests = len(test_examples)
    num_failed = 0

    work_dir = os.path.join(os.getcwd(), "workdir")
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir, onerror=_clear_readonly_bit)

    num_tries = 10
    while True:
        try:
            os.makedirs(work_dir)
            break
        except OSError:
            num_tries -= 1
            if num_tries == 0:
                raise
            time.sleep(0.1)

    print("Number of found test examples: {}".format(num_tests))

    for example in test_examples:
        cwd = os.path.join(work_dir, example)
        os.makedirs(cwd)
        os.chdir(cwd)

        print("Running {}".format(example))

        input_file = os.path.join(test_dir_abs_path, example, "model.ssd")

        ret_val = subprocess.Popen([exe_abs_path, '-v', input_file]).wait()
        if ret_val != 0:
            num_failed += 1
            print("\tFAIL")
            continue

        # check reference files
        test_passed = True
        ref_dir = os.path.join(test_dir_abs_path, example, "reference")
        if os.path.exists(ref_dir):
        for res in os.listdir(ref_dir):
            if not res.endswith(".csv"):
                continue

            res_path = os.path.join("results", res)
            ref_path = os.path.join(ref_dir, res)

            if not os.path.exists(res_path):
                print("Results {} are missing".format(res))
                test_passed = False

            res_data = np.genfromtxt(res_path, delimiter=',', skip_header=3)
            ref_data = np.genfromtxt(ref_path, delimiter=',', skip_header=3)
            diff_data = ref_data - res_data

            for i, j in np.ndindex(diff_data.shape):
                if abs(diff_data[i, j]) > 1e-8:
                    print("Results {} do not match".format(res))
                    test_passed = False
                    break

        if test_passed:
            print("\tSUCCESS")
        else:
            num_failed += 1
            print("\tFAIL")

    print("Tests ran: {}".format(num_tests))
    print("Tests failed: {}".format(num_failed))

    return num_failed


def main():
    args = _parse_args()

    return _run_tests(args.executable, args.examples)


if __name__ == "__main__":
    sys.exit(main())
