/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#ifndef WRENCH_STORAGESERVICE_H
#define WRENCH_STORAGESERVICE_H


#include <string>
#include <set>

#include <services/Service.h>

namespace wrench {

    /***********************/
    /** \cond DEVELOPER   **/
    /***********************/

    class Simulation;  // Forward ref
    class WorkflowFile;

    /**
     * @brief Abstract implementation of a storage service.
     */
    class StorageService : public Service {

    public:

        void stop();

        virtual void copyFile(WorkflowFile *file, StorageService *src) = 0;

        virtual void downloadFile(WorkflowFile *file) = 0;

        virtual void uploadFile(WorkflowFile *file) = 0;

        virtual void deleteFile(WorkflowFile *file) = 0;

        double getCapacity();

        double getFreeSpace();

        /***********************/
        /** \cond INTERNAL    **/
        /***********************/


        StorageService(std::string service_name, std::string mailbox_name_prefix, double capacity);

    protected:

        friend class Simulation;

        void addFileToStorage(WorkflowFile *);

        void removeFileFromStorage(WorkflowFile *);

        std::set<WorkflowFile *> stored_files;
        double capacity;
        double occupied_space = 0;

    private:


    };

    /***********************/
    /** \endcond           */
    /***********************/

};


#endif //WRENCH_STORAGESERVICE_H