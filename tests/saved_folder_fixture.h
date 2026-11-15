#pragma once
#include "sorting_controller.h"

// UI workflows that start with destinations exercise a saved folder session.
inline bool saveFolderConfig(const QString& profile,const QString& input,const photocull::DestinationConfig& config){
    photocull::SortingController controller;
    const bool saved=controller.initialize(profile)&&controller.openFolderSession(input)&&controller.configure(config);
    controller.shutdown();return saved;
}
