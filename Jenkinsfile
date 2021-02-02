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
            sh 'export kerneldir=$ASGARD_KERNEL_SRC && ./build.sh --lkm'
            //archiveArtifacts 'build/*.ko'
            //office365ConnectorSend message: "asgard LKM build successfully with kernel version ${kernel_version}", webhookUrl: WEBHOOK
         }
      }
   }
}