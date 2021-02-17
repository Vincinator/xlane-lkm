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
        BUILD_SUCCESS_LKM = 'false'
        BUILD_SUCCESS_DPDK = 'false'
        BUILD_SUCCESS_PLAIN = 'false'

        NEXUS_VERSION = "nexus3"
        NEXUS_PROTOCOL = "http"
        NEXUS_URL = "10.125.1.120:8081"
        NEXUS_REPOSITORY = "asgard"
        NEXUS_CREDENTIALS_ID = "nexus-user-credentials" 
         
      }

  stages {
      
//    stage('Stop build if kernel source can not be found.'){
//       when { expression { KERNEL_SRC_EXIST == 'false' } }
//       steps {
//         echo "$ASGARD_KERNEL_SRC"
//         echo 'No directory found at ${ASGARD_KERNEL_SRC}. Please check if Jenkins has build the Kernel already, and did not clean the Workspace!'
//         sh 'exit 1'
//       }
//    }

    stage('Build Asgard Module'){
        when { expression { KERNEL_SRC_EXIST == 'true' } }
        steps {
          catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {

            echo "$ASGARD_KERNEL_SRC"
            sh 'export kerneldir=$ASGARD_KERNEL_SRC && ./build.sh --lkm'
            archiveArtifacts 'bin/asgard.ko'
            office365ConnectorSend message: "ASGARD Kernel Module build successfully with kernel version ${kernel_version}", webhookUrl: WEBHOOK
          }
        }
        post
        {
          success { script { BUILD_SUCCESS_LKM='true' } }
          failure { script { BUILD_SUCCESS_LKM='false' } }
        }
    }

    stage('Publish Asgard Kernel Module to Nexus Repository'){
        when {
            allOf {
              expression { PUBLISH == 'true' }
              expression { BUILD_SUCCESS_LKM == 'true' }
            }
        }
        steps {
          nexusArtifactUploader artifacts: [[artifactId: 'asgard-lkm', classifier: '', file: 'bin/asgard.ko', type: 'ko']], credentialsId: 'nexus-user-credentials', groupId: 'lab', nexusUrl: '10.125.1.120:8081', nexusVersion: 'nexus3', protocol: 'http', repository: 'cerebro', version: 'latest'
        }
    }

    stage('Build Asgard DPDK'){
        steps {
          catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
            sh './build.sh --dpdk'
            archiveArtifacts 'bin/runner-dpdk'
            office365ConnectorSend message: "ASGARD DPDK version build successfully", webhookUrl: WEBHOOK
          }
        }
        post
        {
          success { script { BUILD_SUCCESS_DPDK='true' } }
          failure { script { BUILD_SUCCESS_DPDK='false' } }
        }
    }

    stage('Publish Asgard DPDK version to Nexus Repository'){
        when {
            allOf {
              expression { PUBLISH == 'true' }
              expression { BUILD_SUCCESS_DPDK == 'true' }
            }
        }
        steps {
          nexusArtifactUploader artifacts: [[artifactId: 'asgard-dpdk', classifier: '', file: 'bin/runner-dpdk', type: 'bin']], credentialsId: 'nexus-user-credentials', groupId: 'lab', nexusUrl: '10.125.1.120:8081', nexusVersion: 'nexus3', protocol: 'http', repository: 'cerebro', version: 'latest'
        }
    }

    stage('Build Asgard Plain'){
        steps {
          catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
            sh './build.sh --plain'
            archiveArtifacts 'bin/runner-plain'
            office365ConnectorSend message: "ASGARD plain version build successfully", webhookUrl: WEBHOOK
          }
        }
        post
        {
          success { script { BUILD_SUCCESS_PLAIN='true' } }
          failure { script { BUILD_SUCCESS_PLAIN='false' } }
    }
        }

    stage('Publish Asgard Plain version to Nexus Repository'){
        when {
            allOf {
              expression { PUBLISH == 'true' }
              expression { BUILD_SUCCESS_PLAIN == 'true' }
            }
        }
        steps {
          nexusArtifactUploader artifacts: [[artifactId: 'asgard-plain', classifier: '', file: 'bin/runner-plain', type: 'bin']], credentialsId: 'nexus-user-credentials', groupId: 'lab', nexusUrl: '10.125.1.120:8081', nexusVersion: 'nexus3', protocol: 'http', repository: 'cerebro', version: 'latest'
        }
    }

    stage ('Starting Deployment job') {
      when {
            allOf {
              expression { BUILD_SUCCESS_DPDK == 'true' }
              expression { BUILD_SUCCESS_PLAIN == 'true' }
              expression { BUILD_SUCCESS_LKM == 'true' }
            }
        }
        steps {
            build '../Deploy - ASGARD/master'
        }
    }




  }


}


