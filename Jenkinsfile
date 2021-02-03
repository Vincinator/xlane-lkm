def kernel_version= "v5.9.8"
def tier = "dev"
def project_folder = "Vincent"
def kernel_project_name ="Asgard_Kernel"


pipeline {
   agent {label 'asgard01-vm1'}
   environment {
        ASGARD_KERNEL_SRC = "${env.WORKSPACE}/../${project_folder}_${kernel_project_name}_${tier}_${kernel_version}"
        KERNEL_SRC_EXIST  = fileExists "../${project_folder}_${kernel_project_name}_${tier}_${kernel_version}"
        WEBHOOK           = credentials('Teams-WebHook-DevOps-Vincent')

        PUBLISH = 'true'
        BUILD_SUCCESS = 'false'

        NEXUS_VERSION = "nexus3"
        NEXUS_PROTOCOL = "http"
        NEXUS_URL = "10.125.1.120:8081"
        NEXUS_REPOSITORY = "asgard"
        NEXUS_CREDENTIALS_ID = "nexus-user-credentials"
         
      }

  stages {
      
    stage('Stop build if kernel source can not be found.'){
       when { expression { KERNEL_SRC_EXIST == 'false' } }
       steps {
         echo "$ASGARD_KERNEL_SRC"
         echo 'No directory found at ${ASGARD_KERNEL_SRC}. Please check if Jenkins has build the Kernel already, and did not clean the Workspace!'
         sh 'exit 1'
       }
    }

    stage('Build Asgard Module'){
        when { expression { KERNEL_SRC_EXIST == 'true' } }
        steps {
          echo "$ASGARD_KERNEL_SRC"
          sh 'export kerneldir=$ASGARD_KERNEL_SRC && ./build.sh --lkm && ls && ls src'
          archiveArtifacts 'bin/asgard.ko'
          office365ConnectorSend message: "ASGARD Kernel Module build successfully with kernel version ${kernel_version}", webhookUrl: WEBHOOK
        }
        post
        {
          success { script { BUILD_SUCCESS='true' } }
          failure { script { BUILD_SUCCESS='false' } }
        }
    }

    stage('Publish Asgard Kernel Module to Nexus Repository'){
        when {
            allOf {
              expression { PUBLISH == 'true' }
              expression { BUILD_SUCCESS == 'true' }
            }
        }
        steps {
          nexusArtifactUploader artifacts: [[artifactId: 'asgard-lkm', classifier: 'ko', file: 'bin/asgard.ko', type: 'ko']], credentialsId: 'nexus-user-credentials', groupId: 'lab.cerebro.asgard', nexusUrl: '10.125.1.120:8081', nexusVersion: 'nexus3', protocol: 'http', repository: 'asgard', version: '1'
        }
    }

  }


}