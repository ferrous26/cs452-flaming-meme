task :default => :build

ELF_NAME = 'm1'
UW_HOME  = ENV['UW_HOME']
UW_USER  = ENV['UW_USER']

def increment_version
  version = File.read('VERSION').to_i + 1
  File.open('VERSION', 'w') do |fd|
    fd.puts version
  end
end

def rsync_command
  "rsync -trulip --exclude '.git/' --exclude './measurement' " +
    "./ uw:#{UW_HOME}/trains"
end

def path_to dest
  paths = case dest
          when :future
            ['/u3/marada/arm-eabi-gcc/bin',
             '/u3/marada/arm-eabi-gcc/libexec/gcc/arm-eabi/4.9.0']
          when :clang
            ['/u/cs444/bin',
             '/u3/marada/arm-eabi-gcc/bin',
             '/u3/marada/arm-eabi-gcc/libexec/gcc/arm-eabi/4.9.0']
          when :cowan
            ['/u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2',
             '/u/wbcowan/gnuarm-4.0.2/arm-elf/bin']
          end
  "export PATH=#{paths.join(':')}:$PATH"
end

desc 'Build ferOS in the CS environment'
task :build, :params do |_, args|
  params = args[:params] || 'psad'

  # individual flags
  env  = []
  env << 'STRICT=yes'    if params.include? 's'
  env << 'BENCHMARK=yes' if params.include? 'b'
  env << 'ASSERT=yes'    if params.include? 'a'
  env << 'DEBUG=yes'     if params.include? 'd'
  env << 'RELEASE=1'     if params.include? '1'
  env << 'RELEASE=2'     if params.include? '2'
  env << 'RELEASE=3'     if params.include? '3'

  cmds = if params.include? 't'
           []
         else
           ['cd trains/']
         end

  if params.include? 'f'
    env  << 'FUTURE=zomg'
    cmds << path_to(:future)
  elsif params.include? 'l'
    env  << 'CLANG=yes'
    cmds << path_to(:clang)
  else
    cmds << path_to(:cowan)
  end

  env  = env.join ' '
  cmds = cmds.reverse.join ' && '
  jobs = params.include?('p') ? '-j32' : '' # parallelize build

  cmds << " && echo Cleaning Environment && make clean && touch Makefile"
  cmds << " && echo Building Fresh Kernel && #{env} make -s #{jobs}"

  if params.include? 't'
    sh cmds
  else
    # we also want to copy the thing to the TFTP directory
    cmds << " && echo Copying kernel to TFTP server"
    cmds << " && cp kernel.elf /u/cs452/tftp/ARM/#{UW_USER}/#{ELF_NAME}.elf"

    increment_version
    sh rsync_command
    sh "ssh uw '#{cmds}'"
  end
end

desc 'Debug build with benchmarks'
task(:bench) { Rake::Task[:build].invoke('psadb') }

desc 'Standard release build'
task(:release) { Rake::Task[:build].invoke('p2s') }

desc 'Standard release build with benchmarking enabled'
task(:release_bench) { Rake::Task[:build].invoke('p2sb') }

desc 'Build with clang'
task(:clang) { Rake::Task[:build].invoke('l2b') }

desc 'Build using GCC from the future'
task(:future) { Rake::Task[:build].invoke('f2b') }

desc 'Make a local release_bench build'
task(:local) { Rake::Task[:build].invoke('p2sbt') }

namespace :md5 do
  def make_md5_for report
    headers = ['Makefile', 'Rakefile', 'orex.ld'] +
      FileList['include/**/*.h']
    impls   = FileList['src/**/*.c'] +
      FileList['src/**/*.asm']

    require 'digest/md5'
    require 'csv'

    md5 = Digest::MD5.new

    CSV.open("report/#{report}/md5_info_headers.csv", 'w') do |csv|
      csv << ['file', 'hash']
      headers.sort.each do |file|
        csv << [file.gsub(/_/, '\_'), md5.hexdigest(File.read file)]
      end
    end

    CSV.open("report/#{report}/md5_info_impls.csv", 'w') do |csv|
      csv << ['file', 'hash']
      impls.sort.each do |file|
        csv << [file.gsub(/_/, '\_'), md5.hexdigest(File.read file)]
      end
    end

    puts "md5sum -> report/#{report}/md5_info_headers.csv"
    puts "md5sum -> report/#{report}/md5_info_impls.csv"
  end

  task(:a0) { make_md5_for 'a0'       }
  task(:k1) { make_md5_for 'kernel1'  }
  task(:k2) { make_md5_for 'kernel2'  }
  task(:k3) { make_md5_for 'kernel3'  }
  task(:k4) { make_md5_for 'kernel4'  }
  task(:p1) { make_md5_for 'project1' }
  task(:p2) { make_md5_for 'project2' }
end

namespace :report do
  def pdf_compile report
    cd "report/#{report}" do
      sh 'pdflatex report.tex'
      sh 'pdflatex report.tex'
      sh 'open report.pdf'
    end
  end

  desc 'Compile the report for A0'
  task(a0: 'md5:a0') { pdf_compile 'a0' }

  4.times do |count|
    count += 1
    desc "Compile the report for Kernel #{count}"
    task("k#{count}" => "md5:k#{count}") {
      pdf_compile "kernel#{count}"
    }
  end

  desc 'Compile the report for Project 1'
  task(p1: 'md5:p1') { pdf_compile 'project1' }
  desc 'Compile the report for Project 2'
  task(p2: 'md5:p2') { pdf_compile 'project2' }
end
