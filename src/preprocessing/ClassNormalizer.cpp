/*
 * Copyright (c) 2015
 * Forschungszentrum Juelich GmbH, Juelich Supercomputing Center
 *
 * This software may be modified and distributed under the terms of BSD-style license.
 *
 * File name: ClassNormalizer.cpp
 *
 * Description: Implementation of class ClassNormalizer
 *
 * Maintainer: p.glock
 *
 * Email: phil.glock@gmail.com
 */

#include <exception>
#include <sstream>
#include <iostream>

#include "preprocessing/ClassNormalizer.h"

namespace juml {
    ClassNormalizer::ClassNormalizer(MPI_Comm comm) : comm_(comm) {
        MPI_Comm_rank(comm, &this->mpi_rank_);
        MPI_Comm_size(comm, &this->mpi_size_);
    }

    void ClassNormalizer::index(const Dataset<int>& y) {
        this->class_mapping_.clear();
        arma::Mat<int> local_class_labels = arma::unique(y.data());

        // send the local number of classes to all processes
        int n_classes = local_class_labels.n_elem;
        int* n_classes_per_processor = new int[this->mpi_size_];
        MPI_Allgather(&n_classes, 1, MPI_INT, n_classes_per_processor, 1, MPI_INT, this->comm_);

        // calculate displacements
        int* displacements = new int[this->mpi_size_];
        displacements[0] = 0;
        int total_n_classes = 0;
        for (int i = 1; i < this->mpi_size_; ++i) {
            total_n_classes += n_classes_per_processor[i - 1];
            displacements[i] = total_n_classes;
        }
        total_n_classes += n_classes_per_processor[this->mpi_size_ - 1];

        // exchange class labels
        int* total_classes = new int[total_n_classes];
        MPI_Allgatherv(local_class_labels.memptr(), n_classes, MPI_INT, total_classes, n_classes_per_processor, displacements, MPI_INT, this->comm_);

        // compute global unique classes
        arma::Col<int> global_classes(total_classes, total_n_classes, false, true);
        this->class_labels_ = arma::unique(global_classes);

        // release mpi buffers
        delete[] total_classes;
        delete[] displacements;
        delete[] n_classes_per_processor;

        // map original classes to normalized ones
        for (int label = 0; label < this->class_labels_.n_elem; ++label) {
            auto original_class = this->class_labels_(label);
            this->class_mapping_[original_class] = label;
        }
    }

    int ClassNormalizer::transform(int class_label) const {
        auto found = this->class_mapping_.find(class_label);
        if (found == this->class_mapping_.end()) {
            std::stringstream message;
            message << "Class " << class_label << " not found";
            throw std::invalid_argument(message.str().c_str());
        }
        return found->second;
    }
} // namespace juml

